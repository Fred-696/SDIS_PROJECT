#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pigpio.h>
#include <mosquitto.h>

#define DEFAULT_BROKER_ADDRESS "127.0.0.1" // Default broker address
#define CLIENTID "Node1"
#define TOPIC1 "ButtonPress"
#define QOS 1

// LED GPIO Pins
#define LED_GPIO_1 16
#define LED_GPIO_2 20
#define LED_GPIO_3 21

struct mosquitto *client;
char *broker_address = DEFAULT_BROKER_ADDRESS;
int leds = 0; // Variable to track LED states (0 -> 1 -> 2 -> 3 -> 0 cycle)

// MQTT connect callback
void on_connect(struct mosquitto *client, void *userdata, int rc) {
    if (rc == 0) {
        printf("Connected to MQTT broker.\n");
        mosquitto_subscribe(client, NULL, TOPIC1, QOS); // Subscribe on successful connection
    } else {
        fprintf(stderr, "Connection failed with code %d.\n", rc);
    }
}

// Function to update LEDs based on 'leds' variable
void update_leds() {
    gpioWrite(LED_GPIO_1, leds >= 1); // Turn on LED1 if leds >= 1
    gpioWrite(LED_GPIO_2, leds >= 2); // Turn on LED2 if leds >= 2
    gpioWrite(LED_GPIO_3, leds == 3); // Turn on LED3 only if leds == 3
}

// MQTT message callback
void on_message(struct mosquitto *client, void *userdata, const struct mosquitto_message *message) {
    if (message->payloadlen > 0) {
        printf("Message received: %s\n", (char *)message->payload);

        // Cycle through LED states (0 -> 1 -> 2 -> 3 -> 0)
        leds = (leds + 1) % 4;

        // Update the LEDs based on the current state of 'leds'
        update_leds();

        printf("LEDs state updated to: %d\n", leds);
    }
}

void print_usage() {
    printf("Usage: ./node -h <broker_address>\n");
}

int main(int argc, char *argv[]) {
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
