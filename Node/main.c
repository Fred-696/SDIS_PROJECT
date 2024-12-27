#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pigpio.h> // pigpio library
#include "MQTTClient.h"

#define ADDRESS     "tcp://192.168.1.227:1883"
#define CLIENTID    "2" // This Node's ClientID
#define TOPIC       "ButtonPress"
#define PAYLOAD     "B1" 
#define QOS         1
#define TIMEOUT     10000L
#define BUTTON_GPIO 14 // GPIO 14 for button input

MQTTClient_deliveryToken deliveredtoken;

void delivered(void *context, MQTTClient_deliveryToken dt) {
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}

void connlost(void *context, char *cause) {
    printf("\nConnection lost\n");
    if (cause) {
        printf("Cause: %s\n", cause);
    }
}

// Function to connect/reconnect to MQTT broker
int mqtt_connect(MQTTClient *client, MQTTClient_connectOptions *conn_opts) {
    int rc;
    while ((rc = MQTTClient_connect(*client, conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect to broker, return code %d. Retrying in 5 seconds...\n", rc);
        sleep(5); // Wait before retrying
    }
    printf("Connected to MQTT broker.\n");
    return rc;
}

int main(int argc, char* argv[]) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;

    // Initialize pigpio for GPIO control
    if (gpioInitialise() < 0) {
        printf("Failed to initialize pigpio\n");
        return EXIT_FAILURE;
    }
    gpioSetMode(BUTTON_GPIO, PI_INPUT);
    gpioSetPullUpDown(BUTTON_GPIO, PI_PUD_UP); // Enable pull-up resistor
    printf("Initialized pigpio\n");

    // Create MQTT Client
    if ((rc = MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to create client, return code %d\n", rc);
        rc = EXIT_FAILURE;
        goto gpio_exit;
    }

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // Set callback functions
    MQTTClient_setCallbacks(client, NULL, connlost, NULL, delivered);
    printf("Set MQTT callback functions\n");

    // Establish initial connection
    if (mqtt_connect(&client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        printf("Terminating\n");
        goto destroy_exit;
    }

    printf("Start Monitoring button on GPIO %d...\n", BUTTON_GPIO);

    int connection_check_timer = 0;

    while (1) {
        // Periodically check the connection every 5 seconds
        if (connection_check_timer >= 5000000) { // 5 seconds in microseconds
            if (!MQTTClient_isConnected(client)) {
                printf("Connection lost! Attempting to reconnect...\n");
                mqtt_connect(&client, &conn_opts); // Reconnect if disconnected
            }
            else {
                printf("its connected\n");
            }
            connection_check_timer = 0; // Reset timer
        }

        // Monitor button presses
        if (gpioRead(BUTTON_GPIO) == 0) { // Button pressed (logic LOW)
            pubmsg.payload = PAYLOAD;
            pubmsg.payloadlen = (int)strlen(PAYLOAD);
            pubmsg.qos = QOS;
            pubmsg.retained = 0;
            deliveredtoken = 0;

            if ((rc = MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
                printf("Failed to publish message, return code %d\n", rc);
            } else {
                printf("B1 pressed! Sent message successfully: '%s' to topic '%s'\n", PAYLOAD, TOPIC);
            }

            // Debounce - wait for release
            while (gpioRead(BUTTON_GPIO) == 0) {
                usleep(50000); // 50ms delay
            }
        }

        // Small delay to prevent CPU overload
        usleep(10000); // 10ms delay
        connection_check_timer += 10000; // Increment timer by 10ms
    }

destroy_exit:
    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);
gpio_exit:
    gpioTerminate();
    return rc;
}
