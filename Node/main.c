#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pigpio.h> // pigpio library
#include "MQTTClient.h"

#define ADDRESS     "tcp://192.168.5.100:1883"
#define CLIENTID    "2" // This Node's ClientID
#define TOPIC       "ButtonPress"
#define PAYLOAD     "B1" 
#define QOS         1
#define TIMEOUT     10000L
#define BUTTON_GPIO 14 // GPIO 14 for button input

MQTTClient_deliveryToken deliveredtoken;
int mqtt_connected = 0; // Tracks MQTT connection status

void delivered(void *context, MQTTClient_deliveryToken dt){
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}

void connlost(void *context, char *cause){
    printf("\nConnection lost. Cause: %s\n", cause ? cause : "Unknown");
    mqtt_connected = 0; // Mark as disconnected
}

// Function to connect/reconnect to MQTT broker
int mqtt_connect(MQTTClient *client, MQTTClient_connectOptions *conn_opts) {
    int rc;
    while ((rc = MQTTClient_connect(*client, conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect to broker, return code %d. Retrying in 5 seconds...\n", rc);
        sleep(5); // Wait before retrying
    }
    printf("Connected to MQTT broker.\n");
    mqtt_connected = 1; // Mark as connected
    return rc;
}

int main(int argc, char* argv[]){
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;

    // Initialize pigpio for GPIO control
    if (gpioInitialise() < 0){
        printf("Failed to initialize pigpio\n");
        return EXIT_FAILURE;
    }
    gpioSetMode(BUTTON_GPIO, PI_INPUT);
    gpioSetPullUpDown(BUTTON_GPIO, PI_PUD_UP); // Enable pull-up resistor

    // Create MQTT Client
    if ((rc = MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS){
        printf("Failed to create client, return code %d\n", rc);
        rc = EXIT_FAILURE;
        goto gpio_exit;
    }

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // Set callback functions
    MQTTClient_setCallbacks(client, NULL, connlost, delivered, NULL);

    // Connect to MQTT broker (with retry logic)
    mqtt_connect(&client, &conn_opts);

    printf("Monitoring button on GPIO %d...\n", BUTTON_GPIO);

    while (1) {
        if (!mqtt_connected) {
            // If disconnected, try to reconnect
            mqtt_connect(&client, &conn_opts);
        }

        if (gpioRead(BUTTON_GPIO) == 0) { // Button pressed (logic LOW)
            pubmsg.payload = PAYLOAD;
            pubmsg.payloadlen = (int)strlen(PAYLOAD);
            pubmsg.qos = QOS;
            pubmsg.retained = 0;
            deliveredtoken = 0;

            if ((rc = MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token)) != MQTTCLIENT_SUCCESS){
                printf("Failed to publish message, return code %d\n", rc);
            } else {
                printf("B1 pressed! Sent message successfully: %s to topic: %s\n", PAYLOAD, TOPIC);
            }

            // Debounce - wait for release
            while (gpioRead(BUTTON_GPIO) == 0) {
                usleep(50000); // 50ms delay
            }
        }
        usleep(10000); // Small delay to prevent CPU overload
    }

destroy_exit:
    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);
gpio_exit:
    gpioTerminate();
    return rc;
}
