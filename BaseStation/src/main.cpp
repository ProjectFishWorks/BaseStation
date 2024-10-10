//Imports
#include <Arduino.h>

#include "credential.h"

#include <WiFi.h>
//#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include "WiFiClientSecure.h"

#include <NodeControllerCore.h>

#include <PubSubClient.h>

#include <ArduinoJson.h>

#include "emailClient.h"

#include <dataLogging.h>
//--------------------------------------

//CAN Bus message IDs for warnings and alerts
#define WARN_ID 0x384 //900
#define ALERT_ID 0x385 //901

//TODO: Temp until final versioning system is implemented
#define baseStationFirmwareVersion 0.01

//TODO: Manual system ID and base station ID, temp untils automatic paring is implemented
#define systemID 0x00
#define baseStationID 0x00

//TODO: MQTT Credentials - temp until these are added to WiFiManager system
#define mqtt_server "ce739858516845f790a6ae61e13368f9.s1.eu.hivemq.cloud"
#define mqtt_username "fishworks-dev"
#define mqtt_password "F1shworks!"

//Time
//TODO: add timezone support to WiFiManager
#define ntpServer "pool.ntp.org"
#define gmtOffset_sec 0
#define daylightOffset_sec 0

#define manifestFileName "/manifest.json"

//Node Controller Core - temp usage coustom code for base station is created
NodeControllerCore core;

//WiFi client
WiFiClientSecure espClient;

//MQTT client
PubSubClient mqttClient(espClient);

//TODO: Log file write queue - not working right now
QueueHandle_t log_file_queue;
#define LOG_FILE_QUEUE_LENGTH 10

//Global data fix for pointer issues, TODO fix this shit
uint64_t data = 0;

//Function that is called when a CAN Bus message is received
void receivedCANBUSMessage(uint8_t nodeID, uint16_t messageID, uint64_t data) {
    Serial.println("Message received callback");
    Serial.println("Sending message to MQTT");

    //Create the MQTT topic string
    String topic = "out/" + String(systemID) + "/" + String(baseStationID) + "/" + String(nodeID) + "/" + String(messageID);
    
    // Allocate the JSON document
    JsonDocument doc;

    //Add the current time and the data from the CAN Bus message to the JSON doc
    time_t now;
    time(&now);
    doc["time"] = now;
    doc["data"] = data;

    //Convert the JSON Doc to text
    String payload = String(doc.as<String>());

    //Send the message to the MQTT broker
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    //Check for alerts and warnings
    if(messageID == WARN_ID || messageID == ALERT_ID) {
      //Only send an email if the alrt or warning message being turned on
      if(data != 0){
        Serial.println("Alert or Warning message received!!");

        //Send to email queue
        AlertData alertData;
        alertData.nodeID = nodeID;
        alertData.isWarning = messageID == WARN_ID ? 1 : 0;

        sendEmailToQueue(&alertData);
        //xQueueSend(emailQueue, &alertData, portMAX_DELAY);
      }

      //TODO: Trigger hardware alert on the base station

    }

    // //Log the CAN Bus message to the SD card

    //Log the message to the SD card

    writeLogData(systemID, baseStationID, String(baseStationFirmwareVersion), nodeID, messageID, data);

}

//Function to connect to the MQTT broker
void MQTTConnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    //TODO: use the system ID and base station ID to create a unique client ID?
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    // Attempt to connect
    if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      //Subscribe to the MQTT topic for this base station
      String topicIn = "in/" + String(systemID) + "/" + String(baseStationID) + "/#";
      String topicHistory = "historyIn/" + String(systemID) + "/" + String(baseStationID) + "/#";
      String topicManifest = "manifestIn/" + String(systemID) + "/" + String(baseStationID);
      mqttClient.subscribe(topicIn.c_str());
      mqttClient.subscribe(topicHistory.c_str());
      mqttClient.subscribe(topicManifest.c_str());

      //Send manifest data to the MQTT broker
      if(SD.exists(manifestFileName)) {
        Serial.println("Manifest file exists, sending to MQTT");
        File manifestFile = SD.open(manifestFileName, FILE_READ);
        String manifestString = manifestFile.readString();
        manifestFile.close();
        String topic = "manifestOut/" + String(systemID) + "/" + String(baseStationID);

        //Send the manifest data to the MQTT broker
        mqttClient.publish(topic.c_str(), manifestString.c_str(), true);

      }else{
        Serial.println("Manifest file does not exist");
      }


    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void receivedMQTTMessage(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  //Parse the topic to get the node ID and message ID with the format:
  // in/systemID/baseStationID/nodeID/messageID/
  String topicString = String(topic);
  int index = 0;
  index = topicString.indexOf("/", index);
  String type = topicString.substring(0, index);
  index = topicString.indexOf("/", index + 1);
  index = topicString.indexOf("/", index + 1);
  int nodeID = topicString.substring(index + 1, topicString.indexOf("/", index + 1)).toInt();
  index = topicString.indexOf("/", index + 1);
  int messageID = topicString.substring(index + 1, topicString.indexOf("/", index + 1)).toInt();
  
  Serial.print("Type: ");
  Serial.println(type);
  Serial.print("Node ID: ");
  Serial.println(nodeID);
  Serial.print("Message ID: ");
  Serial.println(messageID);

  // Allocate the JSON document
  JsonDocument doc;

  // Parse the JSON object
  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds
  if(error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  //Don't use doc["data"].as<uint64_t>() because it will cause a zero to be sent over the CAN bus
  //TODO fix this global variable hack
  data = doc["data"];
  Serial.print("Data: ");
  Serial.println(data);

  //Regular message
  if(type == "in"){
    //Set the node ID in the Node Controller Core to the node ID of the received message
    //TODO: This is a workaround until we move the Node Controller Core code to the base station codebase
    core.nodeID = nodeID;

    //Send the message to the CAN Bus
    core.sendMessage(messageID, &data);

    //Log the message to the SD card

    writeLogData(systemID, baseStationID, String(baseStationFirmwareVersion), nodeID, messageID, data);

  }
  //History message
  else if(type == "historyIn"){
    Serial.println("History message received for node: " + String(nodeID) + " message: " + String(messageID) + "hours: " + String(data));

    //Send the requested history data to the MQTT broker, data is the number of hours to read
    JsonDocument historyDoc;
    sendLogData(systemID, baseStationID, nodeID, messageID, data, &mqttClient);

  }else if(type == "manifestIn"){
    Serial.println("Manifest message received");

    //Send the requested history data to the MQTT broker, data is the number of hours to read

    if(SD.exists(manifestFileName)) {
      Serial.println("Manifest file exists, replacing");
      SD.remove(manifestFileName);
      File manifestFile = SD.open(manifestFileName, FILE_WRITE);
      manifestFile.write(payload, length);
      manifestFile.close();
    }else
    {
      Serial.println("Manifest file does not exist, creating new file");
      File manifestFile = SD.open(manifestFileName, FILE_WRITE);
      manifestFile.write(payload, length);
      manifestFile.close();
    }

    //Send the manifest data to the MQTT broker

    Serial.println("Sending manifest data to MQTT");
    String manifestTopic = "manifestOut/" + String(systemID) + "/" + String(baseStationID);

    // Allocate the JSON document
    JsonDocument manifestDoc;

    // Parse the JSON object
    DeserializationError error = deserializeJson(manifestDoc, payload);

    Serial.println("Sending manifest data to MQTT:" + manifestDoc.as<String>());

    mqttClient.publish(manifestTopic.c_str(), manifestDoc.as<String>().c_str(), true);

  }
  else {
    Serial.println("Unknown message type");
  }

}

void setup() {
    //Start the serial connection
    Serial.begin(115200);

    pinMode(11, OUTPUT);

    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

    //WiFi.begin("White Rabbit", "2511560A7196");
    WiFi.begin("IoT-Security", "B@kery204!");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    //WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    //wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

   /*  bool res;
    //TODO generate a unique name for the base station based on the system ID and base station ID
    res = wm.autoConnect("Project Fish Works Base Station");
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
    } */

    // Start the NTP Client
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    //For some reson if I don't include a call to getLocalTime here, the time is not set correctly and we get hour values of larger than 24
    struct tm timeinfo;
    getLocalTime(&timeinfo);


    //TODO: is this were we should start the Node Controller Core?
    // Start the Node Controller
    //Set the WiFi client to use the CA certificate from HiveMQ
    //Got it from here: https://community.hivemq.com/t/frequently-asked-questions-hivemq-cloud/514
    espClient.setCACert(CA_cert);  

    //Set the MQTT server and port
    mqttClient.setServer(mqtt_server, 8883);

    //Set the MQTT callback function
    mqttClient.setCallback(receivedMQTTMessage);

    //The max payload size we can send. This seems to be the max size you can use.
    mqttClient.setBufferSize(34464);

    //TODO: Add a check to see if the SD card is still mounted, if not remount it
    //TODO: Add a check to see if the WiFi is still connected, if not reconnect
    initSDCard();

    initEmailClient();

    //Start the Node Controller Core
    core = NodeControllerCore();
    if(core.Init(receivedCANBUSMessage, 0x00)){
        Serial.println("Node Controller Core Started");
    } else {
        Serial.println("Node Controller Core Failed to Start");
    }

    digitalWrite(11, HIGH);

}

void loop() {

    //TODO: Move the MQTT connection to a separate task
    //If the MQTT is not connected, or has disconnect4ed for some reason, connect to the MQTT broker
    if (!mqttClient.connected()) {
        MQTTConnect();
    }
    mqttClient.loop();

    emailClientLoop();

}