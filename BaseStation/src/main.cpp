#include <Arduino.h>

#include "credential.h"

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include "WiFiClientSecure.h"

#include <NodeControllerCore.h>

#include <PubSubClient.h>

#include <ArduinoJson.h>                                      


NodeControllerCore core;

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

#define systemID 0x00
#define baseStationID 0x00

const char* mqtt_server = "ce739858516845f790a6ae61e13368f9.s1.eu.hivemq.cloud";

const char* mqtt_username = "fishworks-dev";
const char* mqtt_password = "F1shworks!";

//Time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

//Global data fix for pointer issues
uint64_t data = 0;

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time!");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void receive_message(uint8_t nodeID, uint16_t messageID, uint64_t data) {
    Serial.println("Message received callback");
    Serial.println("Sending message to MQTT");
    String topic = "out/" + String(systemID) + "/" + String(baseStationID) + "/" + String(nodeID) + "/" + String(messageID);
    
    // Allocate the JSON document
    JsonDocument doc;

    time_t now;

    // Add values in the document
    time(&now);
    doc["time"] = now;
    doc["data"] = data;

    String payload = String(doc.as<String>());

    mqttClient.publish(topic.c_str(), payload.c_str());
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //mqttClient.publish("ECET260/outSebastien", "hello world");
      // ... and resubscribe
      String topic = "in/" + String(systemID) + "/" + String(baseStationID) + "/#";
      mqttClient.subscribe(topic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  // in/systemID/baseStationID/nodeID/messageID/in
  String topicString = String(topic);
  int index = 0;
  index = topicString.indexOf("/", index);
  index = topicString.indexOf("/", index + 1);
  index = topicString.indexOf("/", index + 1);
  int nodeID = topicString.substring(index + 1, topicString.indexOf("/", index + 1)).toInt();
  index = topicString.indexOf("/", index + 1);
  int messageID = topicString.substring(index + 1, topicString.indexOf("/", index + 1)).toInt();
  
  Serial.print("Node ID: ");
  Serial.println(nodeID);
  Serial.print("Message ID: ");
  Serial.println(messageID);

  // Allocate the JSON document
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if(error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  //Don't use doc["data"].as<uint64_t>() because it will cause a zero to be sent over the CAN bus
  data = doc["data"];
  Serial.print("Data: ");
  Serial.println(data);

  core.nodeID = nodeID;
  core.sendMessage(messageID, &data);
  

}


void setup() {
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.

    // put your setup code here, to run once:
    Serial.begin(115200);
    
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    //wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    res = wm.autoConnect("Project Fish Works Base Station"); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    }

    // Start the Node Controller
    core = NodeControllerCore();

    // Start the Node Controller
    if(core.Init(receive_message, 0x00)){
        Serial.println("Node Controller Core Started");
    } else {
        Serial.println("Node Controller Core Failed to Start");
    }

    espClient.setCACert(CA_cert);  

    mqttClient.setServer(mqtt_server, 8883);
    mqttClient.setCallback(callback);

    // Start the NTP Client
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();

}

void loop() {
    // put your main code here, to run repeatedly:
    if (!mqttClient.connected()) {
        reconnect();
    }
    mqttClient.loop();   
}