//Imports
#include <Arduino.h>

#include <SPI.h>

#include <SD.h>

#include "credential.h"

#include <WiFi.h>
//#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include "WiFiClientSecure.h"

#include <NodeControllerCore.h>

#include <PubSubClient.h>

#include <ArduinoJson.h>

#include "emailClient.h"
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
#define daylightOffset_sec 3600

//SD Card Pins
#define sck 3
#define miso 2
#define mosi 1
#define cs 0

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

//Get the current log file name based on the current time
//Currently creates one log file per hour
void getCurrentLogFilename(char* filename) {
  //Buffer to hold the current date/time
  char timeString[255];
  //Time object
  struct tm timeinfo;
  //If we can't get the current time, add a default filename
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time!");
    strcpy(filename, "log-no-time.csv");
    return;
  }
  
  //Format the data/time
  strftime(timeString, 255, "-%Y-%m-%d-%H.csv", &timeinfo);

  //Combine the date/time with system and base station ID
  String filenameString = "/log-" + String(systemID) + "-" + String(baseStationID) + timeString;;

  //Copy the final string to the filename pointer
  filenameString.toCharArray(filename, filenameString.length() + 1);
}

//Write the header info to the log file
void writeLogHeader(File *file) {
  file->println(("SystemID," + String(systemID)));
  file->println("BaseStationID," + String(baseStationID));
  file->println("BaseStationFirmwareVersion," + String(baseStationFirmwareVersion));

  time_t now;
  time(&now);
  file->println("Created," + String(now));
  file->println("Time,NodeID,MessageID,Data");
}

//Creates a row of data for the log file
String getLogDataRow(File *file, uint8_t nodeID, uint16_t messageID, uint64_t data) {
  time_t now;
  time(&now);
  return String(now) + "," + String(nodeID) + "," + String(messageID) + "," + String(data);
}

//Open the log file and creates a new one if it doesn't exist
void openLogFile(File *file) {
  //Get the current filename based on the current time
  char filename[255];
  getCurrentLogFilename(filename);

  //If the file exists, open it in append mode, otherwise create a new file
  if(SD.exists(filename)) {
    *file = SD.open(filename, FILE_APPEND);
  } else {
    Serial.println("Log file does not exist, creating new file with filename: " + String(filename));
    *file = SD.open(filename, FILE_WRITE);
    writeLogHeader(file);
  }
}

//Not working right now, TODO fix this shit
void  writeLogFileQueueTask(void *queue) {
  File logFile;
  while(1) {
    String logData;
    if(xQueueReceive(*(QueueHandle_t *)queue, &logData, portMAX_DELAY) == pdTRUE) {
      openLogFile(&logFile);
      logFile.println(logData);
      Serial.println("Wrote to log file : " + logData);
    }else {
      logFile.close();
    }
  }
}

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

    // //Create the log data row
    // String logData = getLogDataRow(NULL, nodeID, messageID, data);

    // File logFile;
    // char filename[255];
    // getCurrentLogFilename(filename);
    // //For when we get the log file queue working, for now just write directly to the file
    // //xQueueSend(log_file_queue, &logData, portMAX_DELAY);

    // //Open the log file
    // openLogFile(&logFile);
    // //Write the log data row to the log file
    // logFile.println(logData);
    // Serial.println("Wrote to log file : " + logData);
    // //Close the log file
    // logFile.close();

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

  //Set the node ID in the Node Controller Core to the node ID of the received message
  //TODO: This is a workaround until we move the Node Controller Core code to the base station codebase
  core.nodeID = nodeID;

  //Send the message to the CAN Bus
  core.sendMessage(messageID, &data);

  //Log the message to the SD card

  //Create the log data row
  // String logData = getLogDataRow(NULL, nodeID, messageID, data);
  // File logFile;
  // char filename[255];
  // getCurrentLogFilename(filename);
  // //TODO: For when we get the log file queue working, for now just write directly to the file
  // //xQueueSend(log_file_queue, &logData, portMAX_DELAY);
  // openLogFile(&logFile);
  // logFile.println(logData);
  // Serial.println("Wrote to log file : " + logData);
  // logFile.close();
  

}

void setup() {
    //Start the serial connection
    Serial.begin(115200);

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

    // //Mount the SD Card
    // SPI.begin(sck, miso, mosi, cs);
    // if (!SD.begin(cs)) {
    //     Serial.println("Card Mount Failed");
    // }else{
    //     Serial.println("Card Mount Success");
    // }
    // uint8_t cardType = SD.cardType();

    // if (cardType == CARD_NONE) {
    //   Serial.println("No SD card attached");
    //   return;
    // }

    // Serial.print("SD Card Type: ");
    // if (cardType == CARD_MMC) {
    //   Serial.println("MMC");
    // } else if (cardType == CARD_SD) {
    //   Serial.println("SDSC");
    // } else if (cardType == CARD_SDHC) {
    //   Serial.println("SDHC");
    // } else {
    //   Serial.println("UNKNOWN");
    // }

    // uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    // Serial.printf("SD Card Size: %lluMB\n", cardSize);

    //TODO: For when we get the log file queue working
    //log_file_queue = xQueueCreate(LOG_FILE_QUEUE_LENGTH, sizeof(String));
    //xTaskCreate(writeLogFileQueueTask, "writeLogFileQueueTask", 10000, &log_file_queue, 1, NULL);

    //TODO: is this were we should start the Node Controller Core?
    // Start the Node Controller
    //Set the WiFi client to use the CA certificate from HiveMQ
    //Got it from here: https://community.hivemq.com/t/frequently-asked-questions-hivemq-cloud/514
    espClient.setCACert(CA_cert);  

    //Set the MQTT server and port
    mqttClient.setServer(mqtt_server, 8883);

    //Set the MQTT callback function
    mqttClient.setCallback(receivedMQTTMessage);

    //TODO: Add a check to see if the SD card is still mounted, if not remount it
    //TODO: Add a check to see if the WiFi is still connected, if not reconnect


    initEmailClient();

    //Start the Node Controller Core
    core = NodeControllerCore();
    if(core.Init(receivedCANBUSMessage, 0x00)){
        Serial.println("Node Controller Core Started");
    } else {
        Serial.println("Node Controller Core Failed to Start");
    }


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