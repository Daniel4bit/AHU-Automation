//AHU automation 

#include <Arduino.h>
#include <Ethernet.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include <HttpUpdate.h>
#include <WiFi.h>

// Function declarations
void update();
void parseDataFromMessage(String message);
void MqttCallback(String &topic, String &payload);
void connectMqtt();

EthernetClient ethClient;
MQTTClient mqtt;
StaticJsonDocument<256> json;

int lastStatus = 0;  // 1 = ON, 0 = OFF

// Pin config
const int outputPin = 26;
const int buzzerPin = 13;
const int warningLEDPin = 15;
const int wifiLEDPin = 32;

// Ethernet config
byte mac[] = { 0x74, 0x69, 0x69, 0x2D, 0x30, 0x32 };
IPAddress ip(10, 70, 11, 187);
IPAddress mydns(172, 16, 16, 16);
IPAddress gateway(10, 70, 11, 1);
IPAddress subnet(255, 255, 255, 0);

// Device ID (used in topics)
String deviceId = "Sf001/F001/R003";   // <--- change as needed

// MQTT broker
const char* mqtt_server = "10.70.11.247";
String clientId = "clsrm-" + String(deviceId);
// Topics
String subTopic = "devices/" + String(deviceId) + "/control";
String pubTopic = "devices/" + String(deviceId) + "/status";
String statusTopic="devices/" + String(deviceId) + "/lastwill";

// WiFi client for OTA
WiFiClient wifiClient;

void update() {
  String url = "http://cibikomberi.local:8080/thing/update/677367065ee782724fdba1a4?version=1";
  httpUpdate.update(wifiClient, url);
}

// Parse incoming MQTT message
void parseDataFromMessage(String message) {
  DeserializationError error = deserializeJson(json, message);

  if (!error) {
    String command = json["command"].as<String>();

    if (command == "ON") {
      digitalWrite(outputPin, HIGH);
      lastStatus = 1;
      Serial.println("Device turned ON");
    }
    else if (command == "OFF") {
      digitalWrite(outputPin, LOW);
      lastStatus = 0;
      Serial.println("Device turned OFF");
    }

    // Publish status ack
StaticJsonDocument<128> publishDoc;
publishDoc["status"] = (lastStatus == 1 ? "ON" : "OFF");
String payload;
serializeJson(publishDoc, payload);

mqtt.publish(pubTopic.c_str(), payload.c_str(), true, 1);

  }
}

// Called when message received
void MqttCallback(String &topic, String &payload) {
  Serial.print("Msg[");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(payload);

  parseDataFromMessage(payload);
}

// Ensure MQTT connection
void connectMqtt() {
  int i = 5;
  while (!mqtt.connected() && i > 0) {
    Serial.print("MQTT..");

    mqtt.setWill(statusTopic.c_str(), "device disconnected", true, 1);

    if (mqtt.connect(clientId.c_str())) {
      Serial.println("MQTT ok");
      mqtt.publish(statusTopic.c_str(), "{\"status\":\"device connected\"}", true, 1);
      delay(2000);
      mqtt.subscribe(subTopic);
      Serial.print("Subscribed to: ");
      Serial.println(subTopic);
    } else {
      Serial.println("fail");
    }
    i--;
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(outputPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(warningLEDPin, OUTPUT);
  pinMode(wifiLEDPin, OUTPUT);

  digitalWrite(outputPin, LOW);
  digitalWrite(buzzerPin, LOW);
  digitalWrite(warningLEDPin, LOW);
  digitalWrite(wifiLEDPin, LOW);

  Ethernet.init(5);
  Ethernet.begin(mac, ip, mydns, gateway, subnet);

  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());
  digitalWrite(wifiLEDPin, HIGH);

  mqtt.begin(mqtt_server, 1883, ethClient);
  mqtt.onMessage(MqttCallback);
  connectMqtt();

  WiFi.begin("BIT-ENERGY", "pic-embedded");  // WiFi only for OTA updates
}

void loop() {
  if (!mqtt.connected()) {
    digitalWrite(outputPin, LOW);
    lastStatus = 0;
    Serial.println("Device turned OFF");
    connectMqtt();
  }
  mqtt.loop();

  if (WiFi.status() == WL_CONNECTED) {
    update();
  }

  delay(500);
}