#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pigpio.h>
#include <mosquitto.h>

#define DEFAULT_BROKER_ADDRESS "127.0.0.1" // Default broker address
#define CLIENTID "Node2"
#define TOPIC1 "ButtonPress"
#define QOS 1

// LED GPIO Pins
#define LED_GPIO_1 16
#define LED_GPIO_2 20
#define LED_GPIO_3 21

struct mosquitto *client;
char *broker_address = DEFAULT_BROKER_ADDRESS;

// MQTT connect callback
void on_connect(struct mosquitto *client, void *userdata, int rc) {
    if (rc == 0) {
        printf("Connected to MQTT broker.\n");
        mosquitto_subscribe(client, NULL, TOPIC1, QOS); // Subscribe on successful connection
    } else {
        fprintf(stderr, "Connection failed with code %d.\n", rc);
    }
}

// MQTT message callback
void on_message(struct mosquitto *client, void *userdata, const struct mosquitto_message *message) {
    if (message->payloadlen > 0) {
        printf("Message received: %s\n", (char *)message->payload);

        // LED sequence: Turn on LEDs one by one and then turn them off
        gpioWrite(LED_GPIO_1, 1);
        usleep(500000); // Light up for 0.5 seconds
        gpioWrite(LED_GPIO_1, 0);
        
        gpioWrite(LED_GPIO_2, 1);
        usleep(500000);
        gpioWrite(LED_GPIO_2, 0);
        
        gpioWrite(LED_GPIO_3, 1);
        usleep(500000);
        gpioWrite(LED_GPIO_3, 0);

        printf("LED sequence completed.\n");
    }
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

    // Set LED GPIOs as output and enable pull-down resistors
    gpioSetMode(LED_GPIO_1, PI_OUTPUT);
    gpioSetPullUpDown(LED_GPIO_1, PI_PUD_DOWN);
    
    gpioSetMode(LED_GPIO_2, PI_OUTPUT);
    gpioSetPullUpDown(LED_GPIO_2, PI_PUD_DOWN);
    
    gpioSetMode(LED_GPIO_3, PI_OUTPUT);
    gpioSetPullUpDown(LED_GPIO_3, PI_PUD_DOWN);

    // Initialize Mosquitto
    mosquitto_lib_init();
    client = mosquitto_new(CLIENTID, true, NULL);
    if (!client) {
        fprintf(stderr, "Failed to create Mosquitto client.\n");
        return EXIT_FAILURE;
    }

    // Set callbacks for connection and message handling
    mosquitto_connect_callback_set(client, on_connect);
    mosquitto_message_callback_set(client, on_message);

    // Connect to the broker using the provided or default address
    if (mosquitto_connect(client, broker_address, 1883, 20) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Failed to connect to broker %s.\n", broker_address);
        mosquitto_destroy(client);
        mosquitto_lib_cleanup();
        return EXIT_FAILURE;
    }

    // Start event loop for message handling
    if (mosquitto_loop_start(client) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Failed to start event loop.\n");
        mosquitto_destroy(client);
        mosquitto_lib_cleanup();
        return EXIT_FAILURE;
    }

    printf("Waiting for messages on topic '%s'...\n", TOPIC1);

    // Keep the program running indefinitely
    while (1) {
        sleep(1);
    }

    // Cleanup (this code is never reached in the current design)
    mosquitto_disconnect(client);
    mosquitto_destroy(client);
    mosquitto_lib_cleanup();
    gpioTerminate();
    return 0;
}
