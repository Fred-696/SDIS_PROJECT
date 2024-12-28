// #include "broker.h"

// void print_bytes(const uint8_t *buffer, ssize_t length) {
//     printf("Received Bytes:\n");
//     for (ssize_t i = 0; i < length; i++) {
//         printf("%02X ", buffer[i]); // Print each byte in hexadecimal
//     }
//     printf("\n");
// }

// //DEPOIS adicionar funcao para dar free da memoria alocado por malloc

// int main() {
//     int server_fd; //file descriptor
//     struct sockaddr_in address;    //struct with family, port, address
//     int addrlen = sizeof(address);
//     session running_sessions[MAX_CLIENTS] = {0};                //1 session for each node

//     //loop variables
//     uint8_t  buffer[BUFFER_SIZE] = {0};    //buffer
//     int conn_fd = 0;

//     // ============================Initialize broker server==================================//
//     if (create_tcpserver(&server_fd, &address, &addrlen) < 0){
//         exit(EXIT_FAILURE);
//     };
//     // =====================================================================================//
       
//     while (1) {
//         if (conn_fd == 0){
//             if ((conn_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0){
//                 perror("Connection accepted error");
//                 exit(EXIT_FAILURE);
//             }
//             printf("New connection: conn_fd = %d || ", conn_fd);
//         }

//         //read data from client
//         ssize_t valread;
//         if ((valread = read(conn_fd, buffer, BUFFER_SIZE)) < 0){
//             perror("Reading error\n");
//         }

//         mqtt_pck received_pck = {0};
//         received_pck.conn_fd = conn_fd; //associate filedescriptor with package

//         //Process MQTT packet
//         if (mqtt_process_pck(buffer, received_pck, running_sessions) < 0){
//             printf("MQTT Process Error\n");
//         };

//         usleep(10);
//         printf("/////////////\n");
//     }

//     return 0;
// }





/////////////////////////////////////////////////////////////////////////////////////////////




// #include "broker.h"
// //DEPOIS adicionar funcao para dar free da memoria alocado por malloc
// void print_bytes(const uint8_t *buffer, ssize_t length) {
//     printf("Received Bytes:\n");
//     for (ssize_t i = 0; i < length; i++) {
//         printf("%02X ", buffer[i]); // Print each byte in hexadecimal
//     }
//     printf("\n");
// }

// //opa alternativa a isto é meter threads, mas nunca mexi muito nisso
// int make_socket_non_blocking(int fd) {
//     int flags = fcntl(fd, F_GETFL, 0);
//     if (flags == -1) {
//         perror("fcntl(F_GETFL)");
//         return -1;
//     }
//     if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
//         perror("fcntl(F_SETFL)");
//         return -1;
//     }
//     return 0;
// }

// int main() {
//     int server_fd; //file descriptor
//     struct sockaddr_in address;    //struct with family, port, address
//     int addrlen = sizeof(address);
//     session running_sessions[MAX_CLIENTS] = {0};                //1 session for each node

//     //loop variables
//     uint8_t  buffer[BUFFER_SIZE] = {0};    //buffer
//     int conn_fds[MAX_CLIENTS] = {0}; //hold all the connections file descriptors

//     // ============================Initialize broker server==================================//
//     if (create_tcpserver(&server_fd, &address, &addrlen) < 0){
//         exit(EXIT_FAILURE);
//     };
//     // =====================================================================================//
       
//     while (1) {
//         //=============================accept new connections================//
//         int conn_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
//         if (conn_fd >= 0) {
//             // Make the new socket non-blocking
//             if (make_socket_non_blocking(conn_fd) < 0) {
//                 printf("Failed to set non-blocking mode for conn_fd = %d\n", conn_fd);
//                 close(conn_fd);
//                 continue;
//             }
//             // Find a slot for the new connection
//             int slot_idx = -1;
//             for (int i = 0; i < MAX_CLIENTS; i++) {
//                 if (conn_fds[i] == 0) { // if empty
//                     conn_fds[i] = conn_fd;
//                     printf("New connection added: conn_fd = %d at slot %d\n", conn_fd, i);
//                     slot_idx = i;
//                     break;
//                 }
//             }
//             if (slot_idx == -1) {
//                 printf("Maximum connections reached. Closing conn_fd = %d\n", conn_fd);
//                 close(conn_fd); // reject the new connection
//             }
//         } else if (conn_fd == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
//             perror("Accept error");
//         }

//         //============================loop through active connections========================//
//         printf("aaaaaaaaaaa\n");
//         for (int i = 0; i < MAX_CLIENTS; i++) {
//             printf("i: %d\n", i);
//             if (conn_fds[i] > 0) { // if connection is active
//                 ssize_t valread = read(conn_fds[i], buffer, BUFFER_SIZE);
//                 if (valread > 0) { // if data exists
//                     // Process MQTT package
//                     mqtt_pck received_pck = {0};
//                     received_pck.conn_fd = conn_fds[i];
//                     if (mqtt_process_pck(buffer, received_pck, running_sessions) < 0) {
//                         printf("MQTT Process Error for conn_fd = %d\n", conn_fds[i]);
//                     }
//                     memset(buffer, 0, BUFFER_SIZE);
//                 } else if (valread == 0) { // client disconnected
//                     printf("Client disconnected: conn_fd = %d\n", conn_fds[i]);
//                     close(conn_fds[i]);
//                     conn_fds[i] = 0;
//                 } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                     // no data available on this non-blocking socket
//                     continue;
//                 } else {
//                     perror("Read error");
//                     close(conn_fds[i]);
//                     conn_fds[i] = 0;
//                 }
//             }
//         }
//         usleep(10); // prevent CPU overload
//         printf("///////////////\n");
//     }
//     return 0;
// }