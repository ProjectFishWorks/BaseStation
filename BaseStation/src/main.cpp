//Imports
#include <Arduino.h>

#include "credential.h"

#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include "WiFiClientSecure.h"

#include <BaseStationCore.h>

#include <PubSubClient.h>

#include <ArduinoJson.h>

#include "emailClient.h"

#include <dataLogging.h>

#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

#include "FS.h"
#include <LittleFS.h>
#include <time.h>

//--------------------------------------

//CAN Bus message IDs for warnings and alerts
#define WARN_ID 0x384 //900
#define ALERT_ID 0x385 //901

//TODO: Temp until final versioning system is implemented
#define baseStationFirmwareVersion 0.01

//TODO: Manual system ID and base station ID, temp untils automatic paring is implemented
#define systemID 0x00
#define baseStationID 0x01

//TODO: MQTT Credentials - temp until these are added to WiFiManager system
char mqtt_server[255] = "ce739858516845f790a6ae61e13368f9.s1.eu.hivemq.cloud";
char mqtt_username[255] = "fishworks-dev";
char mqtt_password[255] = "F1shworks!";

char email_recipient[255] = "sebastien@robitaille.info";
char email_recipient_name[255] = "Sebastien Robitaille";

//Time
//TODO: add timezone support to WiFiManager
#define ntpServer "pool.ntp.org"
#define gmtOffset_sec 0
#define daylightOffset_sec 0

#define manifestFileName "/manifest.json"

#define mqttConfigFileName "/config.json"

//flag for saving data
bool shouldSaveConfig = false;

//Screen Savers

bool showScreenSaver = false;
bool endingScreenSaver = false;

//NeoPixel Setup Stuffs
#define PIN         45 // On Trinket or Gemma, suggest changing this to 1
#define NUMPIXELS   2 // Popular NeoPixel ring size
#define DELAYVAL    500 // Time (in milliseconds) to pause between pixels


//LCD Screen Setup Stuffs
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define NUMFLAKES     3 // Number of snowflakes in the animation example
#define LOGO_HEIGHT   48
#define LOGO_WIDTH    64
static const unsigned char PROGMEM logo_bmp[] =
/**
 * Made with Marlin Bitmap Converter
 * https://marlinfw.org/tools/u8glib/converter.html
 *
 * This bitmap from the file 'image1 (2).bmp'
 */
{
  B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,
  B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,
  B00000000,B00000000,B00000000,B11111111,B11111100,B00000000,B00000000,B00000000,
  B00000000,B00000000,B00000111,B00000011,B00000011,B10000000,B00000000,B00000000,
  B00000000,B00000000,B00011000,B11110000,B01111100,B11100000,B00000000,B00000000,
  B00000000,B00000000,B00100011,B00001111,B11000011,B00110000,B00000000,B00000000,
  B00000000,B00000000,B01001100,B11100000,B00011100,B11011000,B00000000,B00000000,
  B00000000,B00000000,B01110011,B00011111,B11100011,B00110000,B00000000,B00000000,
  B00000000,B00000000,B00000100,B11000000,B00011100,B10000000,B00000000,B00000000,
  B00000000,B00000000,B00001111,B00011100,B11100110,B10000000,B00000000,B00000000,
  B00000000,B00000000,B00000000,B01000111,B10011001,B00000000,B00000000,B00000000,
  B00000000,B00000000,B00000000,B10011000,B01100100,B00000000,B00000000,B00000000,
  B00000000,B00000000,B00000000,B11100111,B00011000,B00000000,B00000000,B00000000,
  B00000000,B00000000,B00000000,B00001100,B10000000,B00000000,B00000000,B00000000,
  B00000000,B00000000,B00000000,B00001100,B10000000,B00000000,B00000000,B00000000,
  B00000000,B00000000,B00000000,B00000111,B10000000,B00000000,B00000000,B00000000,
  B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,
  B00000001,B11111111,B11111111,B11111111,B11111111,B11111111,B11111111,B11110000,
  B00000001,B10000000,B00000000,B00000000,B00000000,B00000000,B00001100,B00110000,
  B00000001,B10111100,B00110000,B00000000,B00000111,B11110000,B00001000,B00110000,
  B00000001,B10000000,B00000000,B01111111,B00000000,B00000011,B11111111,B11110000,
  B00000001,B00011000,B00000000,B11010011,B11101111,B11000000,B00000000,B00010000,
  B00000001,B00011000,B00000001,B10111101,B11111101,B00110000,B00000000,B00010000,
  B00000001,B00000000,B00000111,B10000000,B11111001,B01001000,B00000000,B00010000,
  B00000001,B00000000,B00011100,B00000000,B00011111,B11100100,B00001110,B00010000,
  B00000001,B00000000,B11100100,B00000000,B00010001,B11111100,B00110111,B00010000,
  B00000001,B00000001,B00110010,B00000000,B00010000,B00111100,B11111001,B10010000,
  B00000001,B00000110,B10011011,B00000000,B00010000,B00011101,B11111001,B10010000,
  B00000001,B00001101,B11101001,B00000000,B00000000,B00010100,B01111001,B10010000,
  B00000001,B10011101,B11101001,B00000000,B00010000,B00010100,B01111111,B10010000,
  B00000001,B10011110,B11001001,B00011111,B10010000,B00010100,B01111001,B10010000,
  B00000001,B10011010,B00001001,B01111100,B11010000,B00011100,B11111101,B10010000,
  B00000001,B10001110,B00001001,B01011100,B01010000,B00011111,B11100001,B10110000,
  B00000000,B10000111,B00110010,B01111100,B01010000,B01111100,B11100001,B00110000,
  B00000000,B10000000,B11100100,B00011111,B11010011,B11111100,B00111011,B00100000,
  B00000000,B11000000,B00111100,B00000011,B11111111,B11100100,B00001110,B00100000,
  B00000000,B01000000,B00000011,B11111111,B11111111,B10001000,B00000000,B01000000,
  B00000000,B00100000,B00000001,B11110110,B00111110,B01110000,B00000000,B11000000,
  B00000000,B00110000,B00000000,B11100010,B00000011,B10000000,B00000000,B10000000,
  B00000000,B00011000,B00000000,B01110010,B00000000,B00000000,B00000001,B00000000,
  B00000000,B00001100,B00000000,B00011110,B00000000,B00000000,B00000010,B00000000,
  B00000000,B00000111,B11110000,B00000000,B00000111,B11110000,B01111100,B00000000,
  B00000000,B00000011,B11111111,B00000000,B11111100,B00111100,B00111000,B00000000,
  B00000000,B00000111,B11111111,B11111111,B11111111,B11111111,B11111100,B00000000,
  B00000000,B00000110,B00000000,B00000000,B00000000,B00000000,B00000100,B00000000,
  B00000000,B00011111,B11111111,B11111111,B11111111,B11111111,B11111111,B00000000,
  B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,
  B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,B00000000
};

// { 0b00000000, 0b11000000,
//   0b00000001, 0b11000000,
//   0b00000001, 0b11000000,
//   0b00000011, 0b11100000,
//   0b11110011, 0b11100000,
//   0b11111110, 0b11111000,
//   0b01111110, 0b11111111,
//   0b00110011, 0b10011111,
//   0b00011111, 0b11111100,
//   0b00001101, 0b01110000,
//   0b00011011, 0b10100000,
//   0b00111111, 0b11100000,
//   0b00111111, 0b11110000,
//   0b01111100, 0b11110000,
//   0b01110000, 0b01110000,
//   0b00000000, 0b00110000 };

int LCD_SDA = 13;
int LCD_SCL = 14;
int CUR_SDA = 9;
int CUR_SCL = 10;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_INA219 ina219(0x4A);
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save mqtt config");
  shouldSaveConfig = true;
}

//Node Controller Core - temp usage coustom code for base station is created
BaseStationCore core;

//WiFi client
WiFiClientSecure espClient;

//MQTT client
PubSubClient mqttClient(espClient);

//TODO: Log file write queue - not working right now
QueueHandle_t log_file_queue;
#define LOG_FILE_QUEUE_LENGTH 10

//Global data fix for pointer issues, TODO fix this shit
//uint64_t data = 0;

//MQTT Loop Task
void mqttLoop(void* _this); 

void screenSaverTask(void *parameters);

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

    delay(10);

    //mqttClient.loop();



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
      if(LittleFS.exists(manifestFileName)) {
        Serial.println("Manifest file exists, sending to MQTT");
        File manifestFile = LittleFS.open(manifestFileName, FILE_READ);
        String manifestString = manifestFile.readString();
        manifestFile.close();
        String topic = "manifestOut/" + String(systemID) + "/" + String(baseStationID);

        //Send the manifest data to the MQTT broker
        mqttClient.publish(topic.c_str(), manifestString.c_str(), true);

      }else{
        Serial.println("Manifest file does not exist");
      }

      digitalWrite(11, HIGH);

      /* //Send manifest data to the MQTT broker
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
      } */


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
/*   Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println(); */

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
  uint64_t data = doc["data"];
  Serial.print("Data: ");
  Serial.println(data);

  //Regular message
  if(type == "in"){
    //Set the node ID in the Node Controller Core to the node ID of the received message
    //TODO: This is a workaround until we move the Node Controller Core code to the base station codebase
    Serial.print("Sending message with Node ID: ");
    Serial.print(nodeID, HEX);
    Serial.print(" Message ID: ");
    Serial.print(messageID, HEX);
    Serial.print(" Data: ");
    Serial.println(data, HEX);

    //Create a CAN Bus message
    twai_message_t message = core.create_message(messageID, nodeID, &data);

    if (twai_transmit(&message, 2000) == ESP_OK) 
    {
      Serial.println("Test Message queued for transmission");
    } 
    else 
    {
      Serial.println("Test Failed to queue message for transmission");
    }

    //Log the message to the SD card
    //mqttClient.loop();
    //writeLogData(systemID, baseStationID, String(baseStationFirmwareVersion), nodeID, messageID, data);

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

    if(LittleFS.exists(manifestFileName)) {
      Serial.println("Manifest file exists, replacing");
      LittleFS.remove(manifestFileName);
      File manifestFile = LittleFS.open(manifestFileName, FILE_WRITE);
      manifestFile.write(payload, length);
      manifestFile.close();
    }else
    {
      Serial.println("Manifest file does not exist, creating new file");
      File manifestFile = LittleFS.open(manifestFileName, FILE_WRITE);
      manifestFile.write(payload, length);
      manifestFile.close();
    }

    /* if(SD.exists(manifestFileName)) {
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
    } */

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

void annoyingBuzz() {
  //for(int i = 0; i < 1; i++){
    digitalWrite(48, HIGH);
    Serial.println("Bzzzzz");
    delay(50);
    digitalWrite(48, LOW);
    //delay(100);
  //}
}

//Test stuff for LCD Screen
void testdrawbitmap(void) {
  display.clearDisplay();

  display.drawBitmap(
    //(display.width()  - LOGO_WIDTH ) / 2,
    random(1 - (LOGO_WIDTH / 2), display.width()),
    //(display.height() - LOGO_HEIGHT) / 2,
    random(1 - (LOGO_HEIGHT / 2), display.height()),
    logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
  display.display();
}

//Test stuff for LCD Screen
void screenSaver(void) {
  Serial.println("Screen saver");
  while (digitalRead(21) == HIGH && digitalRead(47) == HIGH) {
    //Serial.println("Screen saver loop");
    pixels.setPixelColor(1, pixels.Color(0, 0, 0));
    pixels.show();
    display.clearDisplay();
    display.drawBitmap(
    //(display.width()  - LOGO_WIDTH ) / 2,
    random(1, (display.width() - LOGO_WIDTH)),
    //(display.height() - LOGO_HEIGHT) / 2,
    random(1, (display.height() - LOGO_HEIGHT)),
    logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
    display.display();
    for (int i = 0; i < 150; i++) {
      if (digitalRead(21) == LOW || digitalRead(47) == LOW) {
        break;
        }
      delay(16);
      //Serial.println(i);
      pixels.setPixelColor(0, pixels.Color((150 - (i)), 0, 0));
      pixels.show();
    }
    for (int i = 0; i < 150; i++) {
      if (digitalRead(21) == LOW || digitalRead(47) == LOW) {
        break;
        }
      delay(16);
      //Serial.println(i);
      pixels.setPixelColor(0, pixels.Color((0 + (i)), 0, 0));
      pixels.show();
    }
  }
  Serial.println("Screen saver off");
  pixels.setPixelColor(0, pixels.Color(150, 0, 0));
  pixels.show();
}

#define XPOS   0 // Indexes into the 'icons' array in function below
#define YPOS   1
#define DELTAY 2

void testanimate(const uint8_t *bitmap, uint8_t w, uint8_t h) {
  int8_t f, icons[NUMFLAKES][3];

  // Initialize 'snowflake' positions
  for(f=0; f< NUMFLAKES; f++) {
    icons[f][XPOS]   = random(1 - LOGO_WIDTH, display.width());
    icons[f][YPOS]   = -LOGO_HEIGHT;
    icons[f][DELTAY] = random(1, 6);
    Serial.print(F("x: "));
    Serial.print(icons[f][XPOS], DEC);
    Serial.print(F(" y: "));
    Serial.print(icons[f][YPOS], DEC);
    Serial.print(F(" dy: "));
    Serial.println(icons[f][DELTAY], DEC);
  }

  for(int i=0; i<50; i++) { // Loop forever...
    display.clearDisplay(); // Clear the display buffer

    // Draw each snowflake:
    for(f=0; f< NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, SSD1306_WHITE);
    }

    display.display(); // Show the display buffer on the screen
    delay(200);        // Pause for 1/10 second

    // Then update coordinates of each flake...
    for(f=0; f< NUMFLAKES; f++) {
      icons[f][YPOS] += icons[f][DELTAY];
      // If snowflake is off the bottom of the screen...
      if (icons[f][YPOS] >= display.height()) {
        // Reinitialize to a random position, just off the top
        icons[f][XPOS]   = random(1 - LOGO_WIDTH, display.width());
        icons[f][YPOS]   = -LOGO_HEIGHT;
        icons[f][DELTAY] = random(1, 6);
      }
    }
  }
}

void testCurrentSense () {
  Serial.println("Test current sense");
  float shuntvoltage = 0;
  float busvoltage = 0;
  float current_mA = 0;
  float loadvoltage = 0;
  float power_mW = 0;

  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  power_mW = ina219.getPower_mW();
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  
  Serial.print("Bus Voltage:   "); Serial.print(busvoltage); Serial.println(" V");
  Serial.print("Shunt Voltage: "); Serial.print(shuntvoltage); Serial.println(" mV");
  Serial.print("Load Voltage:  "); Serial.print(loadvoltage); Serial.println(" V");
  Serial.print("Current:       "); Serial.print(current_mA); Serial.println(" mA");
  Serial.print("Power:         "); Serial.print(power_mW); Serial.println(" mW");
  Serial.println("");

  display.clearDisplay();
  display.setTextSize(2); // Draw 2X-scale text
  display.setCursor(1, 10);
  display.println(F("Current is"));
  display.setTextSize(1); // Draw 1X-scale text
  display.setCursor(1, 30);
  display.print(current_mA);
  display.print(" mA");
  display.display();      // Show initial text
  delay(2000);

  //testanimate(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT);    // Draw a small bitmap image
  Serial.println("Success");
}
//Yeah, There is lots of it.

//Create Screen Saver Task
void createScreenSaverTask () {
  xTaskCreatePinnedToCore(
    screenSaverTask, /* Function to implement the task */
    "ScreenSaverTask", /* Name of the task */
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    1,  /* Priority of the task */
    NULL,  /* Task handle. */
    0); /* Core where the task should run */
}

void setup() {
    //Start the serial connection
    Serial.begin(115200);

    pinMode(0, INPUT_PULLUP);  //Program button with pullup
    pinMode(21, INPUT_PULLUP); //ButtBlue
    pinMode(47, INPUT_PULLUP); //ButtWhite
    pinMode(48, OUTPUT); //Buzzer

    pinMode(11, OUTPUT); //CAN Bus Power
    digitalWrite(11, LOW);

    
    Wire.begin(LCD_SDA, LCD_SCL);
    Wire1.begin(CUR_SDA, CUR_SCL);
    //I2Cone.begin(LCD_SDA, LCD_SCL);
    //I2Ctwo.begin(CUR_SDA, CUR_SCL);
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      Serial.println(F("SSD1306 allocation failed"));
      for(;;); // Don't proceed, loop forever
      }
    if (! ina219.begin(&Wire1)) {
      Serial.println("Failed to find INA219 chip");
      while (1) { delay(10); }
    }
    ina219.setCalibration_16V_400mA();



    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.
    display.clearDisplay();
    display.drawBitmap(
      (display.width()  - LOGO_WIDTH ) / 2,
      (display.height() - LOGO_HEIGHT) / 2,
      logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);  
    display.display();
    delay(1000);
    Serial.println(F("Initialized LCD Screen"));


    //NeoPixel Setup Stuffs part 2
    #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
    #endif
    pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
    pixels.setBrightness(20); // Set BRIGHTNESS to about 1/23 (max = 255)
    pixels.clear(); // Set all pixel colors to 'off'
    for (int i=0; i<NUMPIXELS; i++){
      pixels.setPixelColor(i, pixels.Color(150, 150, 0));
    }
    pixels.show(); // Set all pixels to yellow
    Serial.println(F("Initialized NeoPixel"));



    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.println(F("Begining Boot..."));
    display.setCursor(20, 30);
    display.println(F("Welcome to:"));
    display.display(); // Show initial text
    delay(1000);
    display.clearDisplay();
    display.setTextSize(2); // Draw 2X-scale text
    display.setCursor(1, 10);
    display.println(F("Fish Sense"));
    display.setTextSize(1); // Draw 1X-scale text
    display.setCursor(1, 30);
    display.println(F("by Project FishWorks"));
    display.display();      // Show initial text
    delay(2000);

    if (!LittleFS.begin(1)) {
      Serial.println("LittleFS Mount Failed");
      //TODO: stall startup here
      return;
    }

    Serial.println("LittleFS Mount Successful");
    pixels.setPixelColor(0, pixels.Color(150, 0, 0)); //Turn on status neopixel to green
    pixels.show(); // Set status pixel to green
    delay(500);

    if(LittleFS.exists(mqttConfigFileName)) {
      Serial.println("MQTT Config file exists, loading");
      File configFile = LittleFS.open(mqttConfigFileName, FILE_READ);
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, configFile);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
      }
      strcpy(mqtt_server, doc["mqtt_server"]);
      strcpy(mqtt_username, doc["mqtt_username"]);
      strcpy(mqtt_password, doc["mqtt_password"]);
      strcpy(email_recipient, doc["email_recipient"]);
      strcpy(email_recipient_name, doc["email_recipient_name"]);
      configFile.close();
    }else{
      Serial.println("MQTT Config file does not exist, creating new file");
      File configFile = LittleFS.open(mqttConfigFileName, FILE_WRITE);
      JsonDocument doc;
      doc["mqtt_server"] = mqtt_server;
      doc["mqtt_username"] = mqtt_username;
      doc["mqtt_password"] = mqtt_password;
      doc["email_recipient"] = email_recipient;
      doc["email_recipient_name"] = email_recipient_name;
      serializeJson(doc, configFile);
      configFile.close();
    }

    Serial.println("Starting");

    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

    //WiFi.begin("White Rabbit", "2511560A7196");
    //WiFi.begin("IoT-Security", "B@kery204!");

    // while (WiFi.status() != WL_CONNECTED) {
    //     delay(500);
    //     Serial.print(".");
    // }

    // Serial.println("");
    // Serial.println("WiFi connected");
    // Serial.println("IP address: ");
    // Serial.println(WiFi.localIP());
    
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;
    
    WiFiManagerParameter mqtt_header_text("<h4>MQTT Broker Login</h4>");

    WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 255);
    WiFiManagerParameter custom_mqtt_username("username", "MQTT Username", mqtt_username, 255);
    WiFiManagerParameter custom_mqtt_password("password", "MQTT Password", mqtt_password, 255);

    WiFiManagerParameter email_header_text("<h4>Email Alert Recipients</h4>");
    WiFiManagerParameter custom_email_recipient("email", "Email Recipient", email_recipient, 255);
    WiFiManagerParameter custom_email_recipient_name("email_name", "Email Recipient Name", email_recipient_name, 255);

    wm.setAPStaticIPConfig(IPAddress(10,10,1,1), IPAddress(10,10,1,1), IPAddress(255,255,255,0));

    wm.setSaveConfigCallback(saveConfigCallback);

    wm.addParameter(&mqtt_header_text);
    wm.addParameter(&custom_mqtt_server);
    wm.addParameter(&custom_mqtt_username);
    wm.addParameter(&custom_mqtt_password);
    wm.addParameter(&email_header_text);
    wm.addParameter(&custom_email_recipient);
    wm.addParameter(&custom_email_recipient_name);

    
    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    
    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.println(F("Hold both buttons"));
    display.setCursor(20, 30);
    display.println(F("to rst wifi.."));
    display.display(); // Show initial text
    for (int i=0; i<4; i++) {
      pixels.setPixelColor(1, pixels.Color(0, 0, 150));
      pixels.show();   // Send the updated pixel colors to the hardware.
      delay(250); // Pause before next pass through loop
      pixels.setPixelColor(1, pixels.Color(0, 0, 0));
      pixels.show();   // Send the updated pixel colors to the hardware.
      delay(250); // Pause before next pass through loop
    }

    //delay(2000);
    if (digitalRead(21) == LOW && digitalRead(47) == LOW) {
      pixels.setPixelColor(1, pixels.Color(0, 0, 150)); //Turn status pixel to blue
      pixels.show(); // Set status pixel to red
      display.clearDisplay();
      display.setTextSize(1); // Draw 2X-scale text
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(10, 10);
      display.println(F("Reseting Wifi.."));
      display.setCursor(20, 30);
      display.println(F("........"));
      display.display(); // Show initial text
      delay(500);
      display.clearDisplay();
      display.setTextSize(1); // Draw 2X-scale text
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(5, 5);
      display.println(F("Didnt Connect Wifi!"));
      display.setCursor(5, 14);
      display.println(F("Please connect to:"));
      display.setCursor(5, 23);
      display.print(F("< "));
      display.print("Fish Sense Setup");
      display.println(F(" >"));
      display.setCursor(5, 32);
      display.println(F("to enter your WiFi"));
      display.setCursor(5, 41);
      display.println(("at the web page: "));
      display.setCursor(5, 50);
      display.println(F("http://10.10.1.1"));
      display.display();     
      //wm.resetSettings();
      wm.startConfigPortal("Fish Sense Setup");
    }
    pixels.setPixelColor(1, pixels.Color(150, 150, 0)); //Turn status pixel to yellow
    pixels.show(); // Set status pixel to green
    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 30);
    display.println(F("Continuing Boot"));
    display.display(); // Show initial text
    delay(500);
    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

   bool res;
    //TODO generate a unique name for the base station based on the system ID and base station ID

    // Call back function to be called when the WiFiManager enters configuration mode
    wm.setAPCallback([](WiFiManager *myWiFiManager) {
      Serial.println("Entered config mode");
      Serial.println(WiFi.softAPIP());
      Serial.println(myWiFiManager->getConfigPortalSSID());

      pixels.setPixelColor(1, pixels.Color(0, 0, 150)); //Turn status pixel to red
      pixels.show(); // Set status pixel to red

      display.clearDisplay();
      display.setTextSize(1); // Draw 2X-scale text
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(5, 5);
      display.println(F("Didnt Connect Wifi!"));
      display.setCursor(5, 14);
      display.println(F("Please connect to:"));
      display.setCursor(5, 23);
      display.print(F("< "));
      display.print(myWiFiManager->getConfigPortalSSID());
      display.println(F(" >"));
      display.setCursor(5, 32);
      display.println(F("to enter your WiFi"));
      display.setCursor(5, 41);
      display.println(("at the web page: "));
      display.setCursor(5, 50);
      display.print(F("http://"));
      display.println(WiFi.softAPIP());

      display.display();      // Show initial text
      //display.startscrollleft(0x00, 0xFF);
      delay(200);

    });

    res = wm.autoConnect("Fish Sense Setup");
    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.print(".");
        }
        //if you get here you have connected to the WiFi    
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        pixels.setPixelColor(1, pixels.Color(150, 0, 0)); //Turn status pixel to green
        pixels.show(); // Set status pixel to green
        delay(500);
    }

    //read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_username, custom_mqtt_username.getValue());
    strcpy(mqtt_password, custom_mqtt_password.getValue());
    strcpy(email_recipient, custom_email_recipient.getValue());

    //save the custom parameters to FS
    if (shouldSaveConfig) {
      Serial.println("Saving mqtt config");
      if(LittleFS.exists(mqttConfigFileName)){
        LittleFS.remove(mqttConfigFileName);
        Serial.println("Removing old mqtt config file");
      }
      File mqttConfigFile = LittleFS.open(mqttConfigFileName, FILE_WRITE);
      JsonDocument doc;
      doc["mqtt_server"] = mqtt_server;
      doc["mqtt_username"] = mqtt_username;
      doc["mqtt_password"] = mqtt_password;
      doc["email_recipient"] = email_recipient;
      doc["email_recipient_name"] = email_recipient_name;
      serializeJson(doc, mqttConfigFile);
      mqttConfigFile.close();
    }

    // Start the NTP Client
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    //For some reson if I don't include a call to getLocalTime here, the time is not set correctly and we get hour values of larger than 24
    struct tm timeinfo;
    getLocalTime(&timeinfo);


    //TODO: is this were we should start the Node Controller Core?
    // Start the Node Controller
    //Set the WiFi client to use the CA certificate from HiveMQ
    //Got it from here: https://community.hivemq.com/t/frequently-asked-questions-hivemq-cloud/514
    //espClient.setCACert(CA_cert);
    espClient.setInsecure();  

    //Set the MQTT server and port
    mqttClient.setServer(mqtt_server, 8883);

    //Set the MQTT callback function
    mqttClient.setCallback(receivedMQTTMessage);

    //The max payload size we can send. This seems to be the max size you can use.
    mqttClient.setBufferSize(34464);

    mqttClient.setKeepAlive(30);
    mqttClient.setSocketTimeout(30);

    //TODO: Add a check to see if the SD card is still mounted, if not remount it
    //TODO: Add a check to see if the WiFi is still connected, if not reconnect
    initSDCard();

    initEmailClient();

    //Start the Node Controller Core
    core = BaseStationCore();
    if(core.Init(receivedCANBUSMessage, 0x00)){
        Serial.println("Node Controller Core Started");
    } else {
        Serial.println("Node Controller Core Failed to Start");
    }

    digitalWrite(48, LOW); //Turn off the buzzer
    digitalWrite(3, HIGH); //Turn on the LCD Backlight
    digitalWrite(19, HIGH); //Keep LCD on

    pixels.setPixelColor(0, pixels.Color(150, 0, 0)); //Turn on status pixel to green
    pixels.show(); // Set status pixel to green
    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.println(F("Boot Successful"));
    display.setCursor(20, 30);
    display.println(F("System Online"));
    display.display(); // Show initial text
    delay(3000);
    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.println(F("Entering Standby"));
    display.setCursor(20, 30);
    display.println(F("Good Night~~"));
    display.display(); // Show initial text
    delay(1000);
    //LCD Screen Setup Stuffs part 2  

    xTaskCreatePinnedToCore(
        mqttLoop,   /* Function to implement the task */
        "mqttLoop", /* Name of the task */
        10000,      /* Stack size in words */
        NULL,       /* Task input parameter */
        1,          /* Priority of the task */
        NULL,       /* Task handle. */
        0);         /* Core where the task should run */

    showScreenSaver = 1;
    xTaskCreatePinnedToCore(
        screenSaverTask,   /* Function to implement the task */
        "screenSaverTask", /* Name of the task */
        10000,      /* Stack size in words */
        NULL,       /* Task input parameter */
        1,          /* Priority of the task */
        NULL,       /* Task handle. */
        0);         /* Core where the task should run */

}

void loop() {

    //TODO: Move the MQTT connection to a separate task

    //TODO: Move the email client to a separate task????????????????????
    emailClientLoop(email_recipient, email_recipient_name);

    if (digitalRead(0) == LOW) {
      //latching debounce
      while(digitalRead(0) == LOW){
        delay(50);
        }
      }
    if (digitalRead(21) == LOW) {
      Serial.println("Button 21 pressed");
      annoyingBuzz();
      //latching debounce
      while(digitalRead(21) == LOW){
        testCurrentSense();
        delay(50);
        showScreenSaver = 1;
        createScreenSaverTask();
        }
      }
    if (digitalRead(47) == LOW) {
      Serial.println("Button 47 pressed");
      Serial.println("Press button 21 to reset");
      display.clearDisplay();
      display.setTextSize(1); // Draw 2X-scale text
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(10, 10);
      display.println(F("Press button 21"));
      display.setCursor(20, 30);
      display.println(F("to reset Fish Sense"));
      display.display(); // Show initial text
      delay(50);
      annoyingBuzz();
      while (digitalRead(47) == LOW) {
        pixels.setPixelColor(1, pixels.Color(150, 150, 0));
        pixels.show();   // Send the updated pixel colors to the hardware.
        delay(200); // Pause before next pass through loop
        pixels.setPixelColor(1, pixels.Color(0, 0, 0));
        pixels.show();   // Send the updated pixel colors to the hardware.
        delay(200); // Pause before next pass through loop
        if (digitalRead(21) == LOW) {
          //wm.startConfigPortal("Fish Sense Setup");
          //laching debounce
          Serial.println("Both buttons pressed");
          Serial.println("Resetting Fish Sense");
          display.clearDisplay();
          display.setTextSize(1); // Draw 2X-scale text
          display.setTextColor(SSD1306_WHITE);
          display.setCursor(10, 10);
          display.println(F("Reseting Fish Sense"));
          display.setCursor(10, 30);
          display.println(F("Brb......"));
          display.display(); // Show initial text
          for (int i=0; i<3; i++) {
            pixels.setPixelColor(1, pixels.Color(150, 0, 0));
            pixels.show();   // Send the updated pixel colors to the hardware.
            delay(DELAYVAL); // Pause before next pass through loop
            pixels.setPixelColor(1, pixels.Color(0, 0, 0));
            pixels.show();   // Send the updated pixel colors to the hardware.
            delay(DELAYVAL); // Pause before next pass through loop
          }
          ESP.restart();
        }
      } 
      showScreenSaver = 1;
      createScreenSaverTask();
    }
    //screenSaver();
}

void screenSaverTask(void *parameters){

  Serial.println("Screen saver task started");
  
  while(1){
    if (showScreenSaver) {
      delay(50);
      endingScreenSaver = 1;
      Serial.println("Running Screen Saver");
      pixels.setPixelColor(1, pixels.Color(0, 0, 0));
      pixels.show();
      display.clearDisplay();
      display.drawBitmap(
      //(display.width()  - LOGO_WIDTH ) / 2,
      random(1, (display.width() - LOGO_WIDTH)),
      //(display.height() - LOGO_HEIGHT) / 2,
      random(1, (display.height() - LOGO_HEIGHT)),
      logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
      display.display();
      for (int i = 0; i < 150; i++) {
        if (digitalRead(21) == LOW || digitalRead(47) == LOW) {
          //break;
          showScreenSaver = 0;
          vTaskDelete(NULL);
          }
        delay(16);
        pixels.setPixelColor(0, pixels.Color((150 - (i)), 0, 0));
        pixels.show();
      }
      for (int i = 0; i < 150; i++) {
        if (digitalRead(21) == LOW || digitalRead(47) == LOW) {
          //break;
          showScreenSaver = 0;
          vTaskDelete(NULL);
          }
      delay(16);
      pixels.setPixelColor(0, pixels.Color((0 + (i)), 0, 0));
      pixels.show();
    }
  }
  if (endingScreenSaver == 1 && showScreenSaver == 0){
    Serial.println("Screen saver off");
    pixels.setPixelColor(0, pixels.Color(150, 0, 0));
    pixels.show();
    endingScreenSaver = 0;
  }
  delay(100);

}
}

void mqttLoop(void* _this){
  Serial.println("Starting MQTT Loop");
  while(1){
    //If the MQTT is not connected, or has disconnect4ed for some reason, connect to the MQTT broker
    if (!mqttClient.connected()) {
        //digitalWrite(11, LOW);
        MQTTConnect();
    }
    mqttClient.loop();
    delay(10);
  }
  }
  
