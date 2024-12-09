#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Wi-Fi settings
const char* ssid =  "Xiaomi Fred";                //"Vodafone-C27E8D";
const char* password = "123456789";

// MQTT settings
const char* mqtt_server = "192.168.5.200"; // Replace with your broker's IP
const char* mqtt_topic_pub = "sensor/temperature";
const char* mqtt_topic_sub = "test/topic";

WiFiClient espClient;
PubSubClient client(espClient);

// Function to connect to Wi-Fi
void setup_wifi() {
    delay(10);
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(ssid, password);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
        retries++;
        if (retries > 20) { // Timeout after 20 seconds
            Serial.println("\nFailed to connect to Wi-Fi. Restarting...");
            ESP.restart();
        }
    }
    Serial.println("\nWi-Fi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

// Callback for received MQTT messages
void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message received on topic: ");
    Serial.println(topic);

    Serial.print("Message: ");
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    
    Serial.println();
}

// Function to connect to the MQTT broker
void reconnect() {
    // Ensure Wi-Fi is connected
    if (WiFi.status() != WL_CONNECTED) {
        setup_wifi();
    }

    while (!client.connected()) {
        Serial.println("Attempting MQTT connection...");
        if (client.connect("ESP32Client")) { // Use a unique client ID
            Serial.println("Connected to MQTT broker.");
            client.subscribe(mqtt_topic_sub); // Subscribe to the topic
            Serial.print("Subscribed to topic: ");
            Serial.println(mqtt_topic_sub);
        } else {
            Serial.print("Failed to connect to broker, rc=");
            Serial.print(client.state());
            Serial.println(". Retrying in 5 seconds...");
            delay(5000);
        }
    }
}

// Setup function
void setup() {
    Serial.begin(115200);
    setup_wifi();

    client.setServer(mqtt_server, 1883); // Configure MQTT broker
    client.setCallback(callback); // Set the callback for incoming messages
}

// Main loop
void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    // Publish a temperature value every 5 seconds
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 5000) {
        lastTime = millis();

        // Simulate a temperature reading
        float temperature = 25.0 + (rand() % 100) / 10.0; // Random temperature between 25.0 and 35.0
        char tempString[8];
        dtostrf(temperature, 6, 2, tempString); // Convert float to string

        Serial.print("Publishing temperature: ");
        Serial.println(tempString);
        client.publish(mqtt_topic_pub, tempString); // Publish temperature to MQTT
    }
}
