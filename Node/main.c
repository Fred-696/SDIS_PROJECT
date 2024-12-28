#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pigpio.h>
#include <mosquitto.h>

#define ADDRESS "192.168.1.227"
#define CLIENTID "Node2"
#define TOPIC "ButtonPress"
#define PAYLOAD "B1"
#define QOS 1
#define BUTTON_GPIO 14

struct mosquitto *client;

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

int main(int argc, char *argv[]) {
    int rc;

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

    // Connect to the broker
    if (mosquitto_connect(client, ADDRESS, 1883, 20) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Failed to connect to broker.\n");
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
