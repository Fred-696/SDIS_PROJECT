#include <signal.h>
#include "broker.h"

//declare the server_fd as a global variable so the signal handler can access it
int server_fd;

//signal handler function(CHATGPT), for ctrl+C cancelation closing socket
void handle_sigint(int sig) {
    printf("\nCaught signal %d, shutting down gracefully...\n", sig);

    // Close the server socket
    if (server_fd > 0) {
        close(server_fd);
    }

    // Perform other cleanup tasks if necessary

    exit(EXIT_SUCCESS);
}

int main() {
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    session running_sessions[MAX_CLIENTS] = {0};

    //register the SIGINT signal handler(CHATGPT)
    signal(SIGINT, handle_sigint);

    if (create_tcpserver(&server_fd, &address, &addrlen) < 0) {
        exit(EXIT_FAILURE);
    }

    while (1) {
        int conn_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (conn_fd < 0) {
            perror("Connection accept error");
            continue;
        }
        printf("New connection: conn_fd = %d\n", conn_fd);

        //allocate memory for thread data
        thread_data *t_data = (thread_data *)malloc(sizeof(thread_data)); //memory size, then cast to needed type
        if (!t_data) {
            perror("Malloc failed");
            close(conn_fd);
            continue;
        }

        t_data->conn_fd = conn_fd;
        t_data->running_sessions = running_sessions;

        //create a new thread for the client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler, (void *)t_data) != 0) { //cast to void type
            perror("Thread creation failed");
            free(t_data);
            close(conn_fd);
            continue;
        }

        // Detach the thread so it cleans up automatically when done
        pthread_detach(thread_id);
    }

    return 0;
}
