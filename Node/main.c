#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"

#define ADDRESS     "adress do broker"  // Replace with your broker
#define CLIENTID    "MeterIDaquidepois"
#define TOPIC_SUB   "topic1"   // Topic to subscribe to
#define TOPIC_PUB   "topic2"   // Topic to publish to
#define QOS         1          // Quality of Service level
#define TIMEOUT     10000L     // Timeout in milliseconds(long)

volatile int keepRunning = 1; // Flag to control the main loop

// Callback for incoming messages
int messageArrivedCallback(void* context, char* topicName, int topicLen, MQTTClient_message* message) {
    printf("Message received on topic '%s': %s\n", topicName, (char*)message->payload);

    // Example: Publish a message to TOPIC_PUB when a specific condition is met
    if (strcmp((char*)message->payload, "trigger_publish") == 0) {
        MQTTClient* client = (MQTTClient*)context;
        const char* payload = "Message published to topic2!";
        MQTTClient_message pubMessage = MQTTClient_message_initializer;
        MQTTClient_deliveryToken token;

        pubMessage.payload = (char*)payload;
        pubMessage.payloadlen = strlen(payload);
        pubMessage.qos = QOS;
        pubMessage.retained = 0;

        MQTTClient_publishMessage(*client, TOPIC_PUB, &pubMessage, &token);
        printf("Published message to topic '%s': %s\n", TOPIC_PUB, payload);
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return 1; // Return 1 to indicate successful processing
}

int main(int argc, char* argv[]) {
    MQTTClient client;
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    // Set up connection options
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // Connect to the MQTT broker
    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect to broker.\n");
        exit(EXIT_FAILURE);
    }
    printf("Connected to broker at %s\n", ADDRESS);

    // Set up the subscription with callbacks
    MQTTClient_setCallbacks(client, &client, NULL, messageArrivedCallback, NULL);
    if (MQTTClient_subscribe(client, TOPIC_SUB, QOS) != MQTTCLIENT_SUCCESS) {
        printf("Failed to subscribe to topic '%s'.\n", TOPIC_SUB);
        MQTTClient_disconnect(client, TIMEOUT);
        MQTTClient_destroy(&client);
        exit(EXIT_FAILURE);
    }
    printf("Subscribed to topic '%s'\n", TOPIC_SUB);

    // Main loop to keep the client running
    while (keepRunning) {
        // You can add additional logic or other tasks here
    }

    // Clean up and disconnect
    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);

    return 0;
}
