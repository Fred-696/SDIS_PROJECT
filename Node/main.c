#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pigpio.h>
#include <mosquitto.h>

#define DEFAULT_BROKER_ADDRESS "127.0.0.1" // Default broker address
#define CLIENTID "Node2"
#define TOPIC "ButtonPress"
#define PAYLOAD "B1"
#define QOS 1
#define BUTTON_GPIO 14

struct mosquitto *client;
char *broker_address = DEFAULT_BROKER_ADDRESS;

void on_connect(struct mosquitto *client, void *userdata, int rc) {
    if (rc == 0) {
        printf("Connected to MQTT broker.\n");
    } else {
        fprintf(stderr, "Connection failed with code %d.\n", rc);
    }
}

void on_publish(struct mosquitto *client, void *userdata, int mid) {
    printf("Message published (mid=%d).\n", mid);
}

void print_usage() {
    printf("Usage: ./node -h <broker_address>\n");
}

int main(int argc, char *argv[]) {
    int rc;
    int opt;

    // Parse command-line arguments
    while ((opt = getopt(argc, argv, "h:")) != -1) {
        switch (opt) {
            case 'h':
                broker_address = optarg;
                break;
            default:
                print_usage();
                return EXIT_FAILURE;
        }
    }

    printf("Using broker address: %s\n", broker_address);

    // Initialize pigpio
    if (gpioInitialise() < 0) {
        fprintf(stderr, "Failed to initialize pigpio\n");
        return EXIT_FAILURE;
    }
    gpioSetMode(BUTTON_GPIO, PI_INPUT);
    gpioSetPullUpDown(BUTTON_GPIO, PI_PUD_UP);

    // Initialize Mosquitto
    mosquitto_lib_init();
    client = mosquitto_new(CLIENTID, true, NULL);
    if (!client) {
        fprintf(stderr, "Failed to create Mosquitto client.\n");
        return EXIT_FAILURE;
    }

    // Set callbacks
    mosquitto_connect_callback_set(client, on_connect);
    mosquitto_publish_callback_set(client, on_publish);

    // Connect to the broker using the provided or default address
    if (mosquitto_connect(client, broker_address, 1883, 20) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Failed to connect to broker %s.\n", broker_address);
        mosquitto_destroy(client);
        mosquitto_lib_cleanup();
        return EXIT_FAILURE;
    }

    // Start event loop
    if (mosquitto_loop_start(client) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Failed to start event loop.\n");
        mosquitto_destroy(client);
        mosquitto_lib_cleanup();
        return EXIT_FAILURE;
    }

    printf("Monitoring button on GPIO %d...\n", BUTTON_GPIO);
    while (1) {
        if (gpioRead(BUTTON_GPIO) == 0) {
            rc = mosquitto_publish(client, NULL, TOPIC, strlen(PAYLOAD), PAYLOAD, QOS, false);
            if (rc != MOSQ_ERR_SUCCESS) {
                fprintf(stderr, "Failed to publish: %s\n", mosquitto_strerror(rc));
            } else {
                printf("Button pressed! Message published: %s\n", PAYLOAD);
            }
            while (gpioRead(BUTTON_GPIO) == 0) { usleep(5000); } // Debounce
        }
        usleep(10000); // Prevent CPU overload
    }

    mosquitto_disconnect(client);
    mosquitto_destroy(client);
    mosquitto_lib_cleanup();
    gpioTerminate();
    return 0;
}
