#include <Arduino.h>

#include <SPI.h>

#include <SD.h>

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
#define baseStationFirmwareVersion 0.01

const char* mqtt_server = "ce739858516845f790a6ae61e13368f9.s1.eu.hivemq.cloud";

const char* mqtt_username = "fishworks-dev";
const char* mqtt_password = "F1shworks!";

//Time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

//Log file write queue
QueueHandle_t log_file_queue;

#define LOG_FILE_QUEUE_LENGTH 10

//Global data fix for pointer issues
uint64_t data = 0;

//SD Card

int sck = 3;
int miso = 2;
int mosi = 1;
int cs = 0;

void getCurrentLogFilename(char* filename) {
  time_t now;
  char timeString[255];
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time!");
    strcpy(filename, "log-no-time.csv");
    return;
  }

  strftime(timeString, 255, "-%Y-%m-%d-%H.csv", &timeinfo);
  String filenameString = "/log-" + String(systemID) + "-" + String(baseStationID) + timeString;;
  filenameString.toCharArray(filename, filenameString.length() + 1);
}

void writeLogHeader(File *file) {
  file->println(("SystemID," + String(systemID)));
  file->println("BaseStationID," + String(baseStationID));
  file->println("BaseStationFirmwareVersion," + String(baseStationFirmwareVersion));

  time_t now;
  time(&now);
  file->println("Created," + String(now));
  file->println("Time,NodeID,MessageID,Data");
}

String getLogDataRow(File *file, uint8_t nodeID, uint16_t messageID, uint64_t data) {
  time_t now;
  time(&now);
  return String(now) + "," + String(nodeID) + "," + String(messageID) + "," + String(data);
}

void openLogFile(File *file) {
  char filename[255];
  getCurrentLogFilename(filename);

  if(SD.exists(filename)) {
    *file = SD.open(filename, FILE_APPEND);
  } else {
    Serial.println("Log file does not exist, creating new file with filename: " + String(filename));
    *file = SD.open(filename, FILE_WRITE);
    writeLogHeader(file);
  }
}

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

    String logData = getLogDataRow(NULL, nodeID, messageID, data);
    File logFile;
    char filename[255];
    getCurrentLogFilename(filename);
    //xQueueSend(log_file_queue, &logData, portMAX_DELAY);
    openLogFile(&logFile);
    logFile.println(logData);
    Serial.println("Wrote to log file : " + logData);
    logFile.close();

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

    String logData = getLogDataRow(NULL, nodeID, messageID, data);
    File logFile;
    char filename[255];
    getCurrentLogFilename(filename);
    //xQueueSend(log_file_queue, &logData, portMAX_DELAY);
    openLogFile(&logFile);
    logFile.println(logData);
    Serial.println("Wrote to log file : " + logData);
    logFile.close();
  

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

    // Start the NTP Client
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();

    //delay(2000);

    //Start SD Card
    SPI.begin(sck, miso, mosi, cs);
    if (!SD.begin(cs)) {
        Serial.println("Card Mount Failed");
    }else{
        Serial.println("Card Mount Success");
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE) {
      Serial.println("No SD card attached");
      return;
    }

    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
      Serial.println("MMC");
    } else if (cardType == CARD_SD) {
      Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
      Serial.println("SDHC");
    } else {
      Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    //log_file_queue = xQueueCreate(LOG_FILE_QUEUE_LENGTH, sizeof(String));

    //xTaskCreate(writeLogFileQueueTask, "writeLogFileQueueTask", 10000, &log_file_queue, 1, NULL);

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

    Serial.println("Current Log File Name:");
    char filename[255];
    getCurrentLogFilename(filename);
    Serial.println(filename);

}

void loop() {
    // put your main code here, to run repeatedly:
    if (!mqttClient.connected()) {
        reconnect();
    }
    mqttClient.loop();   
}