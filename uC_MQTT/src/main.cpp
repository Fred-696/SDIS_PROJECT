#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// Wi-Fi settings
const char* ssid =  "WiFi ssid"
const char* password = "pass"

// MQTT settings
const char* clientID = "Node3";
const char* mqtt_server = "192.168.1.114"; // Replace with your broker's IP
const char* mqtt_topic_pub = "sensor/temperature";
const char* mqtt_topic_sub = "test/topic";

#define PORT 1883
#define QOS 1


// Define pins for BME280
#define BME_SCL 22
#define BME_SDA 21

AsyncMqttClient mqttClient;
Adafruit_BME280 bme; // Create an instance of the BME280 sensor

void connectToWifi() {
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void onMqttConnect(bool sessionPresent) {
    Serial.println("Connected to MQTT broker.");
    mqttClient.subscribe(mqtt_topic_sub, QOS);
    Serial.print("Subscribed to topic: ");
    Serial.println(mqtt_topic_sub);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    Serial.print("Message received on topic: ");
    Serial.println(topic);
    Serial.print("Message: ");
    for (size_t i = 0; i < len; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    connectToWifi();

    mqttClient.onConnect(onMqttConnect);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.setServer(mqtt_server, PORT);

    Wire.begin(BME_SDA, BME_SCL); // Initialize I2C with defined pins
    if (!bme.begin(0x76)) { // Initialize the BME280 sensor (address 0x76)
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1);
    }

    // Set up client ID
    mqttClient.setClientId(clientID);

    mqttClient.connect();
}

void loop() {
    // Publish a temperature value every 5 seconds
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 2000) {
        lastTime = millis();

        // Read temperature from BME280 sensor
        float temperature = bme.readTemperature();
        char tempString[8];
        dtostrf(temperature, 6, 2, tempString); // Convert float to string

        Serial.print("Publishing temperature: ");
        Serial.println(tempString);
        mqttClient.publish(mqtt_topic_pub, QOS, false, tempString); // Publish temperature to MQTT with QoS 1
    }
}
