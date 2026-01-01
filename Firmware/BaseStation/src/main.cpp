// Imports
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

#include "deviceAuthorization.h"
#include "auth0DeviceAuthorization.h"
#include "mqttLogin.h"

// Constants-----------------------------------------------------------------------------------------------------------------------

// #define DEBUG // Comment out to disable debug messages
// #define DEBUG_BaseStationState // Comment out to disable debug messages
// #define DEBUGTime // Comment out to disable debug messages

#define maxPWM 255 // Maximum PWM value for the LED

// CAN Bus message IDs for base station
#define NODE_ID 0
#define UPDATE_UNIX_TIMESTAMP_MESSAGE_ID 2000
#define UPDATE_TIMEZONE_OFFSET_MESSAGE_ID 2001
#define LED_BRIGHTNESS_MESSAGE_ID 2500
uint64_t LEDBrightness; // Brightness of the NeoPixel
float LEDBrightness_float;
#define IS_ERRORS_MESSAGE_ID 2501
#define IS_NO_ERRORS_MESSAGE_ID 2502
#define RESET_ALERTS_MESSAGE_ID 2503

// Error message flags
uint64_t isErrors;
uint64_t isNoErrors;

// Time
char localTime[64];                                  // Local time string
uint64_t localTimeZoneOffset = 8;                    // Timezone offset
String localTimeZone = "UTC+" + localTimeZoneOffset; // Timezone string
char buf_localTimeZone[8];                           // Char array Buffer for timezone string
struct tm timeinfo;
time_t UNIXtime;
uint64_t lastRTCUpdate = millis();

// CAN Bus message IDs for warnings and alerts
#define WARN_ID 0x384  // 900
#define ALERT_ID 0x385 // 901

// TODO: Temp until final versioning system is implemented
#define baseStationFirmwareVersion 0.01

// TODO: Manual system ID and base station ID, temp untils automatic paring is implemented
#define SYSTEM_ID 0x00      // Leave
#define BASESTATION_ID 0x03 // change for desired base station ID -> 0 = Sebastien Pacific Terrarium, 4 =Kayleb's test station, 3 = Kayleb's Reef

// TODO: MQTT Credentials - temp until these are added to WiFiManager system
char mqtt_server[255] = "projectfishworks.ca";
char mqtt_username[255] = "fishworks-dev";
char mqtt_password[255] = "F1shworks!";

// char email_recipient[255] = "sebastien@robitaille.info";
// char email_recipient_name[255] = "Sebastien Robitaille";

char email_recipient[255] = "kayleb_s@hotmail.com";
char email_recipient_name[255] = "Kayleb Stetsko";

MQTTLogin mqttLogin;

String userID;

TaskHandle_t xMQTTloop = NULL;

// TODO: add timezone support to WiFiManager
#define ntpServer "pool.ntp.org"
#define gmtOffset_sec 0
#define daylightOffset_sec 0

#define manifestFileName "/manifest.json"

#define mqttConfigFileName "/config.json"

// flag for saving data
bool shouldSaveConfig = false;

// Screen Savers
long lastInput;
int lastPosition = 1;

// Buzzer Control
int antiBuzzer = 0;

// NeoPixel Setup Stuffs
#define PIN 45       // On Trinket or Gemma, suggest changing this to 1
#define NUMPIXELS 2  // Popular NeoPixel ring size
#define DELAYVAL 500 // Time (in milliseconds) to pause between pixels
int errorQueued = 0; // Flag to indicate if an error has been queued for the NeoPixel
int nightMode = 0;   // Flag to indicate if night mode is enabled

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);

// LCD Screen Setup Stuffs
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define Fishy_HEIHT 16
#define Fishy_WIDTH 32
#define LOGO_HEIGHT 44
#define LOGO_WIDTH 64
bool screenSaverRunning;
int LCD_SDA = 13;
int LCD_SCL = 14;
static const unsigned char PROGMEM fishy_bmp[] =
    /**
     * Made with Marlin Bitmap Converter
     * https://marlinfw.org/tools/u8glib/converter.html
     */
    {
        B00100000, B00000000, B00000000, B00111000,
        B00000000, B00010000, B00000000, B00101000,
        B00000000, B00001000, B00000000, B00111000,
        B00000011, B00001100, B00000000, B00000000,
        B00000001, B10011111, B00000001, B10000000,
        B00111001, B10100000, B10000001, B10000000,
        B00101001, B11000001, B01000000, B00000000,
        B00111000, B10000000, B00100000, B00000000,
        B00000001, B11000000, B01000000, B00000000,
        B00000001, B10100000, B10000100, B00000000,
        B00000001, B10011111, B00000000, B00000000,
        B00000011, B00001100, B00000000, B00000000,
        B01100000, B00001000, B00000000, B11110000,
        B01100000, B00010000, B00000000, B10010000,
        B00000000, B00000000, B10000000, B10010000,
        B00000000, B00000000, B00000000, B11110000};
static const unsigned char PROGMEM logo_bmp[] =
    /**
     * Made with Marlin Bitmap Converter
     * https://marlinfw.org/tools/u8glib/converter.html
     */
    {
        B00000000, B00000000, B00000000, B11111111, B11111100, B00000000, B00000000, B00000000,
        B00000000, B00000000, B00000111, B00000011, B00000011, B10000000, B00000000, B00000000,
        B00000000, B00000000, B00011000, B11110000, B01111100, B11100000, B00000000, B00000000,
        B00000000, B00000000, B00100011, B00001111, B11000011, B00110000, B00000000, B00000000,
        B00000000, B00000000, B01001100, B11100000, B00011100, B11011000, B00000000, B00000000,
        B00000000, B00000000, B01110011, B00011111, B11100011, B00110000, B00000000, B00000000,
        B00000000, B00000000, B00000100, B11000000, B00011100, B10000000, B00000000, B00000000,
        B00000000, B00000000, B00001111, B00011100, B11100110, B10000000, B00000000, B00000000,
        B00000000, B00000000, B00000000, B01000111, B10011001, B00000000, B00000000, B00000000,
        B00000000, B00000000, B00000000, B10011000, B01100100, B00000000, B00000000, B00000000,
        B00000000, B00000000, B00000000, B11100111, B00011000, B00000000, B00000000, B00000000,
        B00000000, B00000000, B00000000, B00001100, B10000000, B00000000, B00000000, B00000000,
        B00000000, B00000000, B00000000, B00001100, B10000000, B00000000, B00000000, B00000000,
        B00000000, B00000000, B00000000, B00000111, B10000000, B00000000, B00000000, B00000000,
        B00000000, B00000000, B00000000, B00000000, B00000000, B00000000, B00000000, B00000000,
        B00000001, B11111111, B11111111, B11111111, B11111111, B11111111, B11111111, B11110000,
        B00000001, B10000000, B00000000, B00000000, B00000000, B00000000, B00001100, B00110000,
        B00000001, B10111100, B00110000, B00000000, B00000111, B11110000, B00001000, B00110000,
        B00000001, B10000000, B00000000, B01111111, B00000000, B00000011, B11111111, B11110000,
        B00000001, B00011000, B00000000, B11010011, B11101111, B11000000, B00000000, B00010000,
        B00000001, B00011000, B00000001, B10111101, B11111101, B00110000, B00000000, B00010000,
        B00000001, B00000000, B00000111, B10000000, B11111001, B01001000, B00000000, B00010000,
        B00000001, B00000000, B00011100, B00000000, B00011111, B11100100, B00001110, B00010000,
        B00000001, B00000000, B11100100, B00000000, B00010001, B11111100, B00110111, B00010000,
        B00000001, B00000001, B00110010, B00000000, B00010000, B00111100, B11111001, B10010000,
        B00000001, B00000110, B10011011, B00000000, B00010000, B00011101, B11111001, B10010000,
        B00000001, B00001101, B11101001, B00000000, B00000000, B00010100, B01111001, B10010000,
        B00000001, B10011101, B11101001, B00000000, B00010000, B00010100, B01111111, B10010000,
        B00000001, B10011110, B11001001, B00011111, B10010000, B00010100, B01111001, B10010000,
        B00000001, B10011010, B00001001, B01111100, B11010000, B00011100, B11111101, B10010000,
        B00000001, B10001110, B00001001, B01011100, B01010000, B00011111, B11100001, B10110000,
        B00000000, B10000111, B00110010, B01111100, B01010000, B01111100, B11100001, B00110000,
        B00000000, B10000000, B11100100, B00011111, B11010011, B11111100, B00111011, B00100000,
        B00000000, B11000000, B00111100, B00000011, B11111111, B11100100, B00001110, B00100000,
        B00000000, B01000000, B00000011, B11111111, B11111111, B10001000, B00000000, B01000000,
        B00000000, B00100000, B00000001, B11110110, B00111110, B01110000, B00000000, B11000000,
        B00000000, B00110000, B00000000, B11100010, B00000011, B10000000, B00000000, B10000000,
        B00000000, B00011000, B00000000, B01110010, B00000000, B00000000, B00000001, B00000000,
        B00000000, B00001100, B00000000, B00011110, B00000000, B00000000, B00000010, B00000000,
        B00000000, B00000111, B11110000, B00000000, B00000111, B11110000, B01111100, B00000000,
        B00000000, B00000011, B11111111, B00000000, B11111100, B00111100, B00111000, B00000000,
        B00000000, B00000111, B11111111, B11111111, B11111111, B11111111, B11111100, B00000000,
        B00000000, B00000110, B00000000, B00000000, B00000000, B00000000, B00000100, B00000000,
        B00000000, B00011111, B11111111, B11111111, B11111111, B11111111, B11111111, B00000000};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Current Sense Setup Stuffs
float shuntvoltage = 0;
float busvoltage = 0;
float current_mA = 0;
float loadvoltage = 0;
float power_mW = 0;
int CUR_SDA = 9;
int CUR_SCL = 10;

Adafruit_INA219 ina219(0x4A); // I2C address 0x4A or

// Struct for Error Queue
uint8_t errorsQueued = 0;
typedef struct
{
  int16_t nodeID;
  uint8_t isWarning;
  uint8_t isSilenced = 1;
  uint8_t isCleared = 1;
} UIAlertData;

// Array of AlertData
UIAlertData alertQueue[50];

// Base Station Core - temp usage coustom code for base station is created
BaseStationCore core;
uint8_t baseStationState = 0;

// WiFi client
WiFiClientSecure espClient;

// MQTT client
PubSubClient mqttClient(espClient);

// TODO: Log file write queue - not working right now
QueueHandle_t log_file_queue;
#define LOG_FILE_QUEUE_LENGTH 10

// Global data fix for pointer issues, TODO fix this shit
// uint64_t data = 0;

// Put Functon Declarations here -----------------------------------------------------------------------------------
void saveConfigCallback();
void mqttLoop(void *_this);
void neoPixelTask(void *parameters);
void mainUIDisplayTask(void *parameters);
void sendWarningToQueue();
void updateRTC(void *parameters);
void receivedCANBUSMessage(uint8_t nodeID, uint16_t messageID, uint8_t logMessage, uint64_t data);
void MQTTConnect();
void MQTTdisconnect();
void receivedMQTTMessage(char *topic, byte *payload, unsigned int length);
void annoyingBuzz();
void testCurrentSense();
void SendMQTTMessage(uint8_t _systemID, uint8_t _baseStationID, uint8_t _nodeID, uint16_t _messageID, uint64_t _data);
String getTopicComponent(String &topic, int componentIndex);
void handleInternalMessage(int messageID, uint64_t data);
void resetAlerts();
void handleHistoryMessage(int nodeID, int messageID, uint64_t data);
void handleManifestMessage(JsonDocument &doc, byte *payload, unsigned int length);
void sendTimezoneOffsetMessage(uint64_t data);

// Authrise the device

DeviceAuthorization *deviceAuthorization;

Auth0DeviceAuthorization auth0DeviceAuthorization = Auth0DeviceAuthorization();

// Function that is called when a CAN Bus message is received
void receivedCANBUSMessage(uint8_t nodeID, uint16_t messageID, uint8_t logMessage, uint64_t data)
{
  Serial.println("Message received callback");
  Serial.println("Sending message to MQTT");

  // Create the MQTT topic string
  String topic = userID + "/out/" + String(SYSTEM_ID) + "/" + String(BASESTATION_ID) + "/" + String(nodeID) + "/" + String(messageID);

  // Allocate the JSON document
  JsonDocument doc;

  // Add the current time and the data from the CAN Bus message to the JSON doc
  time_t now;
  time(&now);
  doc["time"] = now;
  doc["data"] = data;

  // Convert the JSON Doc to text
  String payload = String(doc.as<String>());

  // Send the message to the MQTT broker
  mqttClient.publish(topic.c_str(), payload.c_str(), true);

  // Check for alerts and warnings
  if (messageID == WARN_ID || messageID == ALERT_ID)
  {
    // Only send an email if the alrt or warning message being turned on
    if (data != 0)
    {
      Serial.println("Alert or Warning message received!!");

      // core.sendMessage(ALERT_ID, &data);

      // Send to email queue
      AlertData alertData;
      alertData.nodeID = nodeID;
      alertData.isWarning = messageID == WARN_ID ? 1 : 0;

      sendEmailToQueue(&alertData);

      // Send to alert queue in UIAlertData, in the first slot where isCleared is set to 1
      for (int i = 0; i < 50; i++)
      {
        if (alertQueue[i].isCleared == 1)
        {
          alertQueue[i].nodeID = nodeID;
          alertQueue[i].isWarning = 0;
          alertQueue[i].isSilenced = 0;
          alertQueue[i].isCleared = 0;
          baseStationState = 4;
          errorQueued = 1;
          delay(10);
          break;
        }
      }

      // xQueueSend(emailQueue, &alertData, portMAX_DELAY);
    }

    // TODO: Trigger hardware alert on the base station
  }

  // //Log the CAN Bus message to the SD card

  // Log the message to the SD card

  if (logMessage)
  {
    Serial.println("Logging message");
    writeLogData(SYSTEM_ID, BASESTATION_ID, String(baseStationFirmwareVersion), nodeID, messageID, data);
  }
  else
  {
    Serial.println("Not logging message");
  }

  delay(10);

  // mqttClient.loop();
}

// Function to connect to the MQTT broker
void MQTTConnect()
{
  // Loop until we're reconnected
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");

    mqttLogin = deviceAuthorization->getMQTTLogin(false);

    // Set the MQTT server and port
    mqttClient.setServer(mqttLogin.getServer(), mqttLogin.getPort());

    // Create a random client ID
    // TODO: use the system ID and base station ID to create a unique client ID?
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    // Attempt to connect
    Serial.print("Connecting to MQTT broker: ");
    Serial.println(mqttLogin.getServer());
    Serial.println(mqttLogin.getPassword());
    if (mqttClient.connect(clientId.c_str(), "", mqttLogin.getPassword().c_str()))
    {
      Serial.println("connected");
      // Subscribe to the MQTT topic for this base station

      userID = auth0DeviceAuthorization.getUserID();
      Serial.println("UserID: " + userID);

      String topicIn = userID + "/in/" + String(SYSTEM_ID) + "/" + String(BASESTATION_ID) + "/#";
      String topicHistory = userID + "/historyIn/" + String(SYSTEM_ID) + "/" + String(BASESTATION_ID) + "/#";
      String topicManifest = userID + "/manifestIn/" + String(SYSTEM_ID) + "/" + String(BASESTATION_ID);

      Serial.println("starting delay for mqtt");
      digitalWrite(11, HIGH);
      delay(8000);
      Serial.println("delay finished");

      mqttClient.subscribe(topicIn.c_str());
      mqttClient.subscribe(topicHistory.c_str());
      mqttClient.subscribe(topicManifest.c_str());

      // Send manifest data to the MQTT broker
      if (LittleFS.exists(manifestFileName))
      {
        Serial.println("Manifest file exists, sending to MQTT");
        File manifestFile = LittleFS.open(manifestFileName, FILE_READ);
        String manifestString = manifestFile.readString();
        manifestFile.close();

        String topic = userID + "/manifestOut/" + String(SYSTEM_ID) + "/" + String(BASESTATION_ID);

        // Send the manifest data to the MQTT broker
        mqttClient.publish(topic.c_str(), manifestString.c_str(), true);
      }
      else
      {
        Serial.println("Manifest file does not exist");
      }

      digitalWrite(11, HIGH);

      /* //Send manifest data to the MQTT broker
      if(SD.exists(manifestFileName)) {
        Serial.println("Manifest file exists, sending to MQTT");
        File manifestFile = SD.open(manifestFileName, FILE_READ);
        String manifestString = manifestFile.readString();
        manifestFile.close();
        String topic = "manifestOut/" + String(SYSTEM_ID) + "/" + String(BASESTATION_ID);

        //Send the manifest data to the MQTT broker
        mqttClient.publish(topic.c_str(), manifestString.c_str(), true);

      }else{
        Serial.println("Manifest file does not exist");
      } */
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
    // start the update RTC task
    xTaskCreatePinnedToCore(
        updateRTC,   /* Function to implement the task */
        "updateRTC", /* Name of the task */
        10000,       /* Stack size in words */
        NULL,        /* Task input parameter */
        1,           /* Priority of the task */
        NULL,        /* Task handle. */
        0);          /* Core where the task should run */
  }
}

void MQTTdisconnect()
{
  mqttClient.disconnect();
  // end the MQTT task
  vTaskDelete(xMQTTloop);
}

void receivedMQTTMessage(char *topic, byte *payload, unsigned int length)
{
  /*   Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println(); */

  // Parse the topic to get the node ID and message ID with the format:
  //  userID/in/SYSTEM_ID/baseStationID/nodeID/messageID/
  String topicString = String(topic);
  int index = 0;
  index = topicString.indexOf("/", index);
  index = topicString.indexOf("/", index);
  String type = topicString.substring(index + 1, topicString.indexOf("/", index + 1));
  index = topicString.indexOf("/", index + 1);
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
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    Serial.println((char *)payload);
  }
  else
  {
    // Don't use doc["data"].as<uint64_t>() because it will cause a zero to be sent over the CAN bus
    // TODO fix this global variable hack
    uint64_t data = doc["data"];
    Serial.print("Data: ");
    Serial.println(data);

    // Regular message
    if (type == "in")
    {
      // Set the node ID in the Node Controller Core to the node ID of the received message
      // TODO: This is a workaround until we move the Node Controller Core code to the base station codebase
      Serial.print("Sending message with Node ID: ");
      Serial.print(nodeID, HEX);
      Serial.print(" Message ID: ");
      Serial.print(messageID, HEX);
      Serial.print(" Data: ");
      Serial.println(data, HEX);

      if (nodeID == NODE_ID)
      {
        handleInternalMessage(messageID, data);
      }

      // Create a CAN Bus message
      twai_message_t message = core.create_message(messageID, nodeID, &data);

      if (twai_transmit(&message, 2000) == ESP_OK)
      {
        Serial.println("Test Message queued for transmission");
      }
      else
      {
        Serial.println("Test Failed to queue message for transmission");
      }

      // Log the message to the SD card
      // mqttClient.loop();
      // writeLogData(SYSTEM_ID, baseStationID, String(baseStationFirmwareVersion), nodeID, messageID, data);
    }
    // History message
    else if (type == "historyIn")
    {
      handleHistoryMessage(nodeID, messageID, data);
    }
    else if (type == "manifestIn")
    {
      handleManifestMessage(doc, payload, length);
    }
    else
    {
      Serial.println("Unknown message type");
    }
  }

  delay(100);
}

void annoyingBuzz()
{
  if (digitalRead(1) == HIGH && antiBuzzer == 0)
  {
    // for(int i = 0; i < 1; i++){
    digitalWrite(48, HIGH);
    // Serial.println("Bzzzzz");
    delay(50);
    digitalWrite(48, LOW);
    // delay(100);
  }
  else
  {
    // Serial.println("Buzzer is off");
  }
}

void testCurrentSense()
{
  // Serial.println("Testing current");
  // shuntvoltage = ina219.getShuntVoltage_mV();
  // busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  // power_mW = ina219.getPower_mW();
  // loadvoltage = busvoltage + (shuntvoltage / 1000);
  // Serial.println("Success");
}

void setup() // ------------------------------------------------------------------------------------------------------------------------------------
{
  // Start the serial connection
  Serial.begin(115200);

  // Setup the onboard GPIO
  pinMode(0, INPUT_PULLUP);  // Program button with pullup
  pinMode(1, INPUT_PULLUP);  // Reset button with pullup
  pinMode(2, INPUT_PULLUP);  // Fast boot toggle with pullup
  pinMode(21, INPUT_PULLUP); // ButtBlue
  pinMode(47, INPUT_PULLUP); // ButtWhite
  pinMode(48, OUTPUT);       // Buzzer
  pinMode(11, OUTPUT);       // CAN Bus Power
  digitalWrite(11, LOW);

  // Setup I2C connections
  Wire.begin(LCD_SDA, LCD_SCL);
  Wire1.begin(CUR_SDA, CUR_SCL);

// Connect and begin the NeoPixels
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif
  pixels.begin();                      // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setBrightness(LEDBrightness); // Set BRIGHTNESS to about 1/23 (max = 255)
  pixels.clear();                      // Set all pixel colors to 'off'
  for (int i = 0; i < NUMPIXELS; i++)
  {
    pixels.setPixelColor(i, pixels.Color(0, 0, 150));
    pixels.setBrightness(LEDBrightness);
  }
  pixels.show(); // Set all pixels to yellow
  Serial.println(F("Initialized NeoPixel"));

  // Connect and beging the LCD Screen
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
  display.setTextColor(SSD1306_WHITE);

  // Start boot sequence, or skip if pin 2 pulled low.
  if (digitalRead(2) == LOW)
  {
    Serial.println("fast boot enabled - skipping useless stuff.");
  }
  else
  {
    // Draw a swimming fishey at the top of the screen
    for (int i = -32; i < Fishy_WIDTH + SCREEN_WIDTH; i++)
    {
      display.clearDisplay();
      display.drawBitmap(
          (i),
          (0),
          fishy_bmp, Fishy_WIDTH, Fishy_HEIHT, 1);
      display.drawBitmap(
          (display.width() - LOGO_WIDTH) / 2,
          ((display.height() / 4) + 2),
          logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
      display.setCursor(120, 54);
      display.print(BASESTATION_ID);
      display.display();
      delay(10);
    }
  }
  Serial.println(F("Initialized LCD Screen"));
  delay(10);

  // Connect and begin the current sensor
  if (!ina219.begin(&Wire1))
  {
    Serial.println("Failed to find INA219 chip");
    // while (1) { delay(10); }
  }
  // Custoum calibration values for the current sensor (ussing the 16V entry as template,
  // it is actually setup for 24v. But not well. Further testing required)
  ina219.setCalibration_16V_400mA();

  // Initial Boot Dialog
  if (digitalRead(2) == HIGH)
  {
    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setCursor(1, 1);
    display.println(F("Welcome to:"));
    display.drawBitmap(
        (display.width() - LOGO_WIDTH) / 2,
        ((display.height() / 4) + 2),
        logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
    display.display(); // Show initial text
    delay(1000);

    display.clearDisplay();
    display.setTextSize(2); // Draw 2X-scale text
    display.setCursor(1, 1);
    display.println(F("Fish Sense"));
    display.drawBitmap(
        (display.width() - LOGO_WIDTH) / 2,
        ((display.height() / 4) + 2),
        logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
    display.display(); // Show initial text
    delay(1500);

    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setCursor(1, 1);
    display.println(F("by Project FishWorks"));
    display.drawBitmap(
        (display.width() - LOGO_WIDTH) / 2,
        ((display.height() / 4) + 2),
        logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
    display.display(); // Show initial text
    delay(1500);
  }

  // Setup the SD Card and File System
  if (!LittleFS.begin(1))
  {
    Serial.println("LittleFS Mount Failed");
    // TODO: stall startup here
    return;
  }
  Serial.println("LittleFS Mount Successful");

  pixels.setPixelColor(0, pixels.Color(10, 0, 0));  // Turn on status neopixel to green
  pixels.setPixelColor(1, pixels.Color(10, 10, 0)); // Turn on status neopixel to yellow
  pixels.setBrightness(LEDBrightness);
  pixels.show(); // Set status pixel to green

  if (LittleFS.exists(mqttConfigFileName))
  {
    Serial.println("MQTT Config file exists, loading");
    File configFile = LittleFS.open(mqttConfigFileName, FILE_READ);
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    if (error)
    {
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
  }
  else
  {
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

  // WiFi.begin("White Rabbit", "2511560A7196");
  // WiFi.begin("IoT-Security", "B@kery204!");

  // while (WiFi.status() != WL_CONNECTED) {
  //     delay(500);
  //     Serial.print(".");
  // }

  // Serial.println("");
  // Serial.println("WiFi connected");
  // Serial.println("IP address: ");
  // Serial.println(WiFi.localIP());

  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  WiFiManagerParameter mqtt_header_text("<h4>MQTT Broker Login</h4>");

  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 255);
  WiFiManagerParameter custom_mqtt_username("username", "MQTT Username", mqtt_username, 255);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT Password", mqtt_password, 255);

  WiFiManagerParameter email_header_text("<h4>Email Alert Recipients</h4>");
  WiFiManagerParameter custom_email_recipient("email", "Email Recipient", email_recipient, 255);
  WiFiManagerParameter custom_email_recipient_name("email_name", "Email Recipient Name", email_recipient_name, 255);

  wm.setAPStaticIPConfig(IPAddress(10, 10, 1, 1), IPAddress(10, 10, 1, 1), IPAddress(255, 255, 255, 0));

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

  if (digitalRead(2) == HIGH)
  {
    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 17);
    display.println(F("Hold both buttons"));
    display.setCursor(20, 31);
    display.println(F("to enter BIOS."));
    display.display(); // Show initial text
    for (int i = 0; i < 4; i++)
    {
      pixels.setPixelColor(0, pixels.Color(10, 0, 0));
      pixels.setPixelColor(1, pixels.Color(0, 0, 10));
      pixels.setBrightness(LEDBrightness);
      pixels.show(); // Send the updated pixel colors to the hardware.
      delay(250);    // Pause before next pass through loop
      pixels.setPixelColor(0, pixels.Color(10, 0, 0));
      pixels.setPixelColor(1, pixels.Color(0, 0, 0));
      pixels.setBrightness(LEDBrightness);
      pixels.show(); // Send the updated pixel colors to the hardware.
      delay(250);    // Pause before next pass through loop
    }

    if (digitalRead(21) == LOW && digitalRead(47) == LOW)
    {
      pixels.setPixelColor(0, pixels.Color(0, 0, 10)); // Turn status pixel to green
      pixels.setPixelColor(1, pixels.Color(0, 0, 10)); // Turn status pixel to blue
      pixels.setBrightness(LEDBrightness);
      pixels.show(); // Set status pixel to red
      display.clearDisplay();
      display.setTextSize(1); // Draw 2X-scale text
      display.setCursor(10, 1);
      display.println(F("Starting BIOS.."));
      display.setCursor(10, 37);
      display.println(F("........"));
      display.setCursor(20, 47);
      display.println(F("........"));
      display.setCursor(30, 57);
      display.println(F("........"));
      display.display(); // Show initial text
      delay(500);
      display.clearDisplay();
      display.setTextSize(2); // Draw 2X-scale text
      display.setCursor(5, 1);
      display.println(F("BIOS Mode"));
      display.setTextSize(1); // Draw 1X-scale text
      display.setCursor(5, 17);
      display.println(F("Please connect to:"));
      display.setCursor(5, 27);
      display.print(F("< "));
      display.print("Fish Sense Setup");
      display.println(F(" >"));
      display.setCursor(5, 37);
      display.println(F("and navigate to"));
      display.setCursor(5, 47);
      display.println(("the web page at"));
      display.setCursor(5, 57);
      display.println(F("http://10.10.1.1"));
      display.display();
      wm.startConfigPortal("Fish Sense Setup");
    }
  }
  pixels.setPixelColor(0, pixels.Color(10, 0, 0));  // Turn status pixel to green
  pixels.setPixelColor(1, pixels.Color(10, 10, 0)); // Turn status pixel to yellow
  pixels.setBrightness(LEDBrightness);
  pixels.show(); // Set status pixel to green
  display.clearDisplay();
  display.setTextSize(1); // Draw 2X-scale text
  display.setCursor(10, 31);
  display.println(F("Continuing Boot"));
  display.display(); // Show initial text
                     // Automatically connect using saved credentials,
                     // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
                     // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
                     // then goes into a blocking loop awaiting configuration and will return success result

  bool res;
  // TODO generate a unique name for the base station based on the system ID and base station ID

  // Call back function to be called when the WiFiManager enters configuration mode
  wm.setAPCallback([](WiFiManager *myWiFiManager)
                   {
                     Serial.println("Entered config mode");
                     Serial.println(WiFi.softAPIP());
                     Serial.println(myWiFiManager->getConfigPortalSSID());

                     pixels.setPixelColor(0, pixels.Color(0, 0, 10)); // Turn status pixel to blue
                     pixels.setPixelColor(1, pixels.Color(0, 0, 10)); // Turn status pixel to blue
                     pixels.setBrightness(LEDBrightness);
                     pixels.show(); // Set status pixel to red

                     display.clearDisplay();
                     display.setTextSize(2); // Draw 2X-scale text
                     display.setCursor(5, 1);
                     display.println(F("No WiFi!"));
                     display.setTextSize(1); // Draw 1X-scale text
                     display.setCursor(5, 17);
                     display.println(F("Please connect to:"));
                     display.setCursor(5, 27);
                     display.print(F("< "));
                     display.print(myWiFiManager->getConfigPortalSSID());
                     display.println(F(" >"));
                     display.setCursor(5, 37);
                     display.println(F("to enter your WiFi"));
                     display.setCursor(5, 47);
                     display.println(("at the web page: "));
                     display.setCursor(5, 57);
                     display.print(F("http://"));
                     display.println(WiFi.softAPIP());
                     display.display(); // Show initial text
                     delay(25); });

  res = wm.autoConnect("Fish Sense Setup");
  if (!res)
  {
    Serial.println("Failed to connect");
    // ESP.restart();
  }
  else
  {
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    // if you get here you have connected to the WiFi
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }

  // read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_username, custom_mqtt_username.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(email_recipient, custom_email_recipient.getValue());

  // save the custom parameters to FS
  if (shouldSaveConfig)
  {
    Serial.println("Saving mqtt config");
    if (LittleFS.exists(mqttConfigFileName))
    {
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
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // Set the time zone and NTP server--------------------------------------------------------------------------------------

  // For some reason if I don't include a call to getLocalTime here, the time is not set correctly and we get hour values of larger than 24
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  time_t UNIXtime = time(NULL);
  //localTimeZone = ("UTC+" + String(localTimeZoneOffset));
  //localTimeZone.toCharArray(buf_localTimeZone, 8);
  //setenv("TZ", buf_localTimeZone, 1);
  //tzset();
  localtime_r(&UNIXtime, &timeinfo);
  strftime(localTime, sizeof(localTime), "%c", &timeinfo);

  // TODO: is this were we should start the Node Controller Core?
  //  Start the Node Controller
  // Set the WiFi client to use the CA certificate from HiveMQ
  // Got it from here: https://community.hivemq.com/t/frequently-asked-questions-hivemq-cloud/514
  // espClient.setCACert(CA_cert);
  espClient.setInsecure();

  // TODO: Add a check to see if the SD card is still mounted, if not remount it
  // TODO: Add a check to see if the WiFi is still connected, if not reconnect

  deviceAuthorization = &auth0DeviceAuthorization;

  // Set the MQTT callback function
  mqttClient.setCallback(receivedMQTTMessage);

  // The max payload size we can send. This seems to be the max size you can use.
  mqttClient.setBufferSize(34464);

  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(30);

  initSDCard();

  initEmailClient();

  // Start the Node Controller Core
  core = BaseStationCore();
  if (core.Init(receivedCANBUSMessage, 0x00))
  {
    Serial.println("Node Controller Core Started");
    pixels.setPixelColor(0, pixels.Color(10, 0, 0)); // Turn on status pixel to green
    pixels.setPixelColor(1, pixels.Color(10, 0, 0)); // Turn on status pixel to green
    pixels.setBrightness(LEDBrightness);
    pixels.show(); // Set status pixel to green
  }
  else
  {
    Serial.println("Node Controller Core Failed to Start");
    pixels.setPixelColor(0, pixels.Color(0, 10, 0)); // Turn on status pixel to green
    pixels.setPixelColor(1, pixels.Color(0, 10, 0)); // Turn on status pixel to green
    pixels.setBrightness(LEDBrightness);
    pixels.show(); // Set status pixel to green
  }

  digitalWrite(48, LOW); // Turn off the buzzer

  if (digitalRead(2) == HIGH)
  {
    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 17);
    display.println(F("Boot Successful"));
    display.setCursor(20, 31);
    display.println(F("System Online"));
    display.display(); // Show initial text
    delay(3000);
    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 17);
    display.println(F("Entering Standby"));
    display.setCursor(20, 31);
    display.println(F("Good Night~~"));
    display.display(); // Show initial text
    delay(1000);
  }

  xTaskCreatePinnedToCore(
      mqttLoop,   /* Function to implement the task */
      "mqttLoop", /* Name of the task */
      30000,      /* Stack size in words */
      NULL,       /* Task input parameter */
      1,          /* Priority of the task */
      &xMQTTloop, /* Task handle. */
      0);         /* Core where the task should run */

  // start the main UI task
  xTaskCreatePinnedToCore(
      mainUIDisplayTask,   /* Function to implement the task */
      "mainUIDisplayTask", /* Name of the task */
      10000,               /* Stack size in words */
      NULL,                /* Task input parameter */
      1,                   /* Priority of the task */
      NULL,                /* Task handle. */
      1);                  /* Core where the task should run */

  // start the NeoPixel task
  xTaskCreatePinnedToCore(
      neoPixelTask,   /* Function to implement the task */
      "neoPixelTask", /* Name of the task */
      10000,          /* Stack size in words */
      NULL,           /* Task input parameter */
      1,              /* Priority of the task */
      NULL,           /* Task handle. */
      1);             /* Core where the task should run */

  baseStationState = 0;
}

void loop() //---------------------------------------------------Loop-----------------------------------------------------------------------------
{
  delay(100);
  emailClientLoop(email_recipient, email_recipient_name);

  // Go to screen saver after 5 seconds should no button be pressed
  if (baseStationState == 1)
  {
    if (millis() >= lastInput + 7000)
    {
      // Serial.println("Screen Saver Timeout");
      baseStationState = 0;
    }
  }

  if (digitalRead(0) == LOW)
  {
    if (digitalRead(11) == HIGH)
    {
      Serial.println("CANBus Off");
      screenSaverRunning = false;
      baseStationState = 5;
      MQTTdisconnect();
      digitalWrite(11, LOW);
    }
    else
    {
      Serial.println("CANBus On");
      screenSaverRunning = false;
      baseStationState = 0;
      xTaskCreatePinnedToCore(
          mqttLoop,   /* Function to implement the task */
          "mqttLoop", /* Name of the task */
          10000,      /* Stack size in words */
          NULL,       /* Task input parameter */
          1,          /* Priority of the task */
          &xMQTTloop, /* Task handle. */
          0);         /* Core where the task should run */
    }
    while (digitalRead(0) == LOW)
    {
      delay(10);
    }
  }
  if (digitalRead(21) == LOW)
  {
    screenSaverRunning = false;
    lastInput = millis();
    while (digitalRead(21) == LOW)
    {
      delay(10);
    }
  }
  if (digitalRead(47) == LOW)
  {
    // Serial.println("Button 47 pressed");
    screenSaverRunning = false;
    lastInput = millis();
    if (baseStationState == 4)
    {
    }
    else
    {
      if (baseStationState > 2)
      {
        baseStationState = 0;
      }
      else
      {
        baseStationState++;
      }
    }
    // Serial.println(baseStationState);
    // latching debounce
    while (digitalRead(47) == LOW)
    {
      delay(10);
    }
  }
  for (int i = 0; i < 50; i++)
  {
    if (alertQueue[i].isSilenced == 0)
    {
      baseStationState = 4;
    }
  }
  delay(25);
}

// function definitions ----------------------------------------------------------------------------------------------------------------------------

void saveConfigCallback()
{
  Serial.println("Running void saveConfigCallback()");
  Serial.println("Should save mqtt config");
  shouldSaveConfig = true;
  Serial.println("");
}



  
// function to handle internal messages from the MQTT broker
void handleInternalMessage(int messageID, uint64_t data)
{
  Serial.println("Running void handleInternalMessage(int messageID, uint64_t data)");
  switch (messageID)
  {
  case LED_BRIGHTNESS_MESSAGE_ID:
    Serial.println("LED Brightness message received");
    LEDBrightness_float = ((maxPWM / 100.0) * data);
    LEDBrightness = (int)LEDBrightness_float;
    pixels.setBrightness(LEDBrightness);
    pixels.show();
    Serial.println("LEDBrightness = ((maxPWM / 100.0) * data)" + String(LEDBrightness));
    Serial.println("");
    break;

  case RESET_ALERTS_MESSAGE_ID:
    Serial.println("Reset alerts message received");
    resetAlerts();
    break;

  case UPDATE_TIMEZONE_OFFSET_MESSAGE_ID:
    localTimeZoneOffset = data;
    localTimeZone = ("UTC+" + String(data));
    sendTimezoneOffsetMessage(data);
    break;

  default:
    Serial.println("Unknown message ID");
    break;
  }
  Serial.println("");
}

void sendTimezoneOffsetMessage(uint64_t data)
{
  Serial.println("Running void sendTimezoneOffsetMessage(uint64_t data)");
  twai_message_t timeZoneOffsetMessage = core.create_message(UPDATE_TIMEZONE_OFFSET_MESSAGE_ID, NODE_ID, &data);
  if (twai_transmit(&timeZoneOffsetMessage, 2000) == ESP_OK)
  {
    Serial.println("Sending Time Zone offset Message, queued for transmission");
    Serial.println("localTimeZoneOffset = " + String(localTimeZoneOffset));
    Serial.println("");
  }
  else
  {
    Serial.println("Failed to Send Time Zone offset Message");
    Serial.println("");
  }
}

// Function to send a message to the MQTT broker
void resetAlerts()
{
  Serial.println("Reset alerts message received");
  isErrors = 0;
  Serial.println("isErrors = 0");
  isNoErrors = 1;
  Serial.println("isNoErrors = 1");
  for (int i = 0; i < 50; i++)
  {
    alertQueue[i].isSilenced = 1;
  }
  SendMQTTMessage(SYSTEM_ID, BASESTATION_ID, NODE_ID, IS_ERRORS_MESSAGE_ID, isErrors);
  delay(100);
  SendMQTTMessage(SYSTEM_ID, BASESTATION_ID, NODE_ID, IS_NO_ERRORS_MESSAGE_ID, isNoErrors);
  delay(100);
  Serial.println("SednMQTTMessage(SYSTEM_ID, BASESTATION_ID, NODE_ID, IS_ERRORS_MESSAGE_ID, isErrors" + String(isErrors));
  Serial.println("SendMQTTMessage(SYSTEM_ID, BASESTATION_ID, NODE_ID, IS_NO_ERRORS_MESSAGE_ID, isNoErrors" + String(isNoErrors));
  Serial.println("Alerts reset mesages to MQTT sent to change the status of the Errors detected and No errors detected chip");
  Serial.println("");
}

// Function to send a message to the MQTT broker
void handleHistoryMessage(int nodeID, int messageID, uint64_t data)
{
  Serial.println("Running void handleHistoryMessage(int nodeID, int messageID, uint64_t data)");
  Serial.println("History message received for node: " + String(nodeID) + " message: " + String(messageID) + " hours: " + String(data));
  JsonDocument historyDoc;
  sendLogData(userID, SYSTEM_ID, BASESTATION_ID, nodeID, messageID, data, &mqttClient);
  Serial.println("History message sent to MQTT");
  Serial.println("");
}

// Function to send a message to the MQTT broker
void handleManifestMessage(JsonDocument &doc, byte *payload, unsigned int length)
{
  Serial.println("Running void handleManifestMessage(JsonDocument &doc, byte *payload, unsigned int length)");
  if (LittleFS.exists(manifestFileName))
  {
    Serial.println("Manifest file exists, removing old file");
    LittleFS.remove(manifestFileName);
    File manifestFile = LittleFS.open(manifestFileName, FILE_WRITE);
    manifestFile.write(payload, length);
    manifestFile.close();
  }
  else
  {
    Serial.println("Manifest file does not exist, creating new file");
    File manifestFile = LittleFS.open(manifestFileName, FILE_WRITE);
    manifestFile.write(payload, length);
    manifestFile.close();
  }

  Serial.println("Sending manifest data to MQTT");
  String manifestTopic = userID + "/manifestOut/" + String(SYSTEM_ID) + "/" + String(BASESTATION_ID);
  mqttClient.publish(manifestTopic.c_str(), doc.as<String>().c_str(), true);
  Serial.println("Manifest data sent to MQTT");
  Serial.println("");
}

/*----------------------------------------------Original function pre compartmetalization--------------------------------------------------------------



void receivedMQTTMessage(char *topic, byte *payload, unsigned int length) // Callback function for MQTT messages----------------------------------------------
{
  /*   Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println(); */

// Parse the topic to get the node ID and message ID with the format:
//  in/systemID/baseStationID/nodeID/messageID/

/*
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
if (error)
{
  Serial.print(F("deserializeJson() failed: "));
  Serial.println(error.c_str());
  Serial.println((char *)payload);
}
else
{
  // Don't use doc["data"].as<uint64_t>() because it will cause a zero to be sent over the CAN bus
  // TODO fix this global variable hack
  uint64_t data = doc["data"];
  Serial.print("Data: ");
  Serial.println(data);

  // Regular message
  if (type == "in")
  {
    // Set the node ID in the Node Controller Core to the node ID of the received message
    // TODO: This is a workaround until we move the Node Controller Core code to the base station codebase
    Serial.print("Sending message with Node ID: ");
    Serial.print(nodeID, HEX);
    Serial.print(" Message ID: ");
    Serial.print(messageID, HEX);
    Serial.print(" Data: ");
    Serial.println(data, HEX);

    if (nodeID == NODE_ID)
    {
      uint8_t systemID = SYSTEM_ID;
      uint8_t baseStationID = BASESTATION_ID;
      uint16_t isErrorsMessageID = IS_ERRORS_MESSAGE_ID;
      uint16_t isNoErrorsMessageID = IS_NO_ERRORS_MESSAGE_ID;
      Serial.println("MQQT Message received to self");
      Serial.println("");
      // Check for messages
      switch (messageID)
      {
      case LED_BRIGHTNESS_MESSAGE_ID:
        LEDBrightness = ((maxPWM / 100.0) * data);
        pixels.setBrightness(LEDBrightness);
        pixels.show();
        Serial.println("LED Brightness message received");
        Serial.println("");
        break;

      case RESET_ALERTS_MESSAGE_ID: // Reset alerts message--------------------------------------------------------------------------------------------
        isErrors = 0;
        isNoErrors = 1;
        Serial.println("Reset alerts message received");
        for (int i = 0; i < 50; i++)
        {
          alertQueue[i].isSilenced = 1;
        }
        SendMQTTMessage(systemID, baseStationID, nodeID, isErrorsMessageID, isErrors);
        delay(100);
        Serial.println("SendMQTTMessage(systemID, baseStationID, nodeID, isErrorsMessageID, isErrors" + String(isErrors));
        SendMQTTMessage(systemID, baseStationID, nodeID, isNoErrorsMessageID, isNoErrors);
        delay(100);
        Serial.println("SendMQTTMessage(systemID, baseStationID, nodeID, isErrorsMessageID, isErrors" + String(isNoErrors));
        Serial.println("Alerts reset mesages to MQTT sent");
        Serial.println("");
        break;

      case UPDATE_TIMEZONE_OFFSET_MESSAGE_ID:
        localTimeZoneOffset = data;
        localTimeZone = "UTC+" + String(localTimeZoneOffset);
        Serial.print("Update timezone message received offset= " + String(data));
        Serial.println("");
        break;

      default:
        Serial.println("Unknown message ID");
        break;
      }
    }
    else
    {
      // Create a CAN Bus message
      twai_message_t message = core.create_message(messageID, nodeID, &data);

      if (twai_transmit(&message, 2000) == ESP_OK)
      {
        Serial.println("Test Message queued for transmission");
      }
      else
      {
        Serial.println("Test Failed to queue message for transmission");
      }

      delay(100);

      // Log the message to the SD card
      // mqttClient.loop();
      // writeLogData(systemID, baseStationID, String(baseStationFirmwareVersion), nodeID, messageID, data);
    }
  }
  // History message
  if (type == "historyIn")
  {
    Serial.println("History message received for node: " + String(nodeID) + " message: " + String(messageID) + "hours: " + String(data));

    // Send the requested history data to the MQTT broker, data is the number of hours to read
    JsonDocument historyDoc;
    sendLogData(SYSTEM_ID, BASESTATION_ID, nodeID, messageID, data, &mqttClient);
  }
  else if (type == "manifestIn")
  {
    Serial.println("Manifest message received");

    if (LittleFS.exists(manifestFileName))
    {
      Serial.println("Manifest file exists, replacing");
      LittleFS.remove(manifestFileName);
      File manifestFile = LittleFS.open(manifestFileName, FILE_WRITE);
      Serial.println(doc.as<String>());
      manifestFile.write(payload, length);
      manifestFile.close();
    }
    else
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

/*
      // Send the manifest data to the MQTT broker

      Serial.println("Sending manifest data to MQTT");
      String manifestTopic = "manifestOut/" + String(SYSTEM_ID) + "/" + String(BASESTATION_ID);

      mqttClient.publish(manifestTopic.c_str(), doc.as<String>().c_str(), true);
    }
    else
    {
      Serial.println("Unknown message type");
    }
  }
  delay(100);
}
*/

// Main UI Switch State Task
void mainUIDisplayTask(void *parameters)
{
  Serial.println("Running void mainUIDisplayTask(void *parameters)");
  long lastScreenSaver;
  long lastCurrentTest;
  long buttPress;
  int errorNumber;
  int state3screen = 0;
  int onlyOnce = 0;
  while (1)
  {
    // switch case for the main UI display
    switch (baseStationState)
    {

    case 0:
// Screen Saver-------------------------------------------------------------------------------------------------------------------------------
#ifdef DEBUG_BaseStationState
      Serial.println("");
      Serial.println("Base Station State case 0 Screen Saver");
      Serial.println("");
      delay(1000);
#endif
      if (nightMode == 0)
      {
        if (screenSaverRunning == false)
        {
          Serial.println("Starting Screen Saver");
          screenSaverRunning = true;
          display.clearDisplay();
          if (lastPosition < (display.width() - LOGO_WIDTH))
          {
            display.drawBitmap(
                timeinfo.tm_sec,
                2,
                logo_bmp,
                LOGO_WIDTH,
                LOGO_HEIGHT,
                1);
            display.setTextSize(2); // Draw 2X-scale text
            display.setCursor(10, 50);
            display.println(String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec));
            lastPosition++;
            display.display();
            lastScreenSaver = millis();

#ifdef DEBUGTIME
            Serial.println("Last Position: " + String(lastPosition));
            Serial.println("");
#endif
          }
          else
          {
            lastPosition = 1;
          }
        }
        if (millis() > lastScreenSaver + 1000)
        {
          // Serial.println("Running Screen Saver");
          display.clearDisplay();
          if (lastPosition < (display.width() - LOGO_WIDTH))
          {
            display.drawBitmap(
                timeinfo.tm_sec,
                2,
                logo_bmp,
                LOGO_WIDTH,
                LOGO_HEIGHT,
                1);
            display.setTextSize(2); // Draw 2X-scale text
            display.setCursor(10, 50);
            display.println(String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec));
            lastPosition++;
            display.display();
            lastScreenSaver = millis();

#ifdef DEBUGTIME
            Serial.println("Last Position: " + String(lastPosition));
            Serial.println("");
#endif
          }
          else
          {
            lastPosition = 1;
          }
        }
      }
      else
      {
        if (screenSaverRunning == false)
        {
          screenSaverRunning = true;
          display.clearDisplay();
          if (lastPosition < (display.width() - LOGO_WIDTH))
          {
            display.drawBitmap(
                timeinfo.tm_sec,
                2,
                logo_bmp,
                LOGO_WIDTH,
                LOGO_HEIGHT,
                1);
            display.setTextSize(2); // Draw 2X-scale text
            display.setCursor(10, 50);
            display.println(String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec));
            lastPosition++;
            display.display();
            lastScreenSaver = millis();

#ifdef DEBUGTIME
            Serial.println("Last Position: " + String(lastPosition));
            Serial.println("");
#endif
          }
          else
          {
            lastPosition = 1;
          }
        }
        if (millis() > lastScreenSaver + 1000)
        {
          display.clearDisplay();
          if (lastPosition <= (display.width() - LOGO_WIDTH))
          {
            display.drawBitmap(
                timeinfo.tm_sec,
                2,
                logo_bmp,
                LOGO_WIDTH,
                LOGO_HEIGHT,
                1);
            display.setTextSize(2); // Draw 2X-scale text
            display.setCursor(10, 50);
            display.println(String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec));
            lastPosition++;
            display.display();
            lastScreenSaver = millis();

#ifdef DEBUGTIME
            Serial.println("Last Position: " + String(lastPosition));
            Serial.println("");
#endif
          }
          else
          {
            lastPosition = 1;
          }
        }
      }
      delay(25);
      break;

    case 1:
// Error Page
#ifdef DEBUG_BaseStationState
      Serial.println("");
      Serial.println("Base Station State case 1 Error Page");
      Serial.println("");
      delay(1000);
#endif
      for (int i = 0; i < 50; i++)
      {
        if (alertQueue[i].isCleared == 0)
        {
          display.clearDisplay();
          display.setTextSize(2); // Draw 2X-scale text
          display.setCursor(1, 1);
          display.print(F("Error "));
          display.println(i + 1);
          display.setTextSize(1); // Draw 1X-scale text
          display.setCursor(10, 17);
          display.println(F("Silenced errors on"));
          display.setCursor(20, 27);
          errorQueued = 1;
          String deviceName = "Node " + String(alertQueue[i].nodeID);

          // Send manifest data to the MQTT broker
          if (LittleFS.exists(manifestFileName))
          {
            // Serial.println("Manifest file exists");
            File manifestFile = LittleFS.open(manifestFileName, FILE_READ);
            String manifestString = manifestFile.readString();
            manifestFile.close();

            JsonDocument manifestDoc;

            // Parse the JSON object
            DeserializationError error = deserializeJson(manifestDoc, manifestString);

            if (error)
            {
              Serial.println("Error parsing manifest file");
            }
            else
            {
              // Get the devices array from the manifest file
              JsonArray devices = manifestDoc["Devices"].as<JsonArray>();
              uint16_t index = 0;
              while (!devices[index].isNull())
              {
                Serial.println("Checking device: " + devices[index]["NodeID"].as<String>());
                JsonObject device = devices[index];
                if (device["NodeID"] == alertQueue[i].nodeID)
                {
                  Serial.println("Device found in manifest file");
                  deviceName = device["DeviceName"].as<String>();
                  break;
                }
                index++;
              }
            }
          }
          else
          {
            Serial.println("Manifest file does not exist");
          }

          display.println(deviceName);
          display.setCursor(10, 37);
          display.println(F("Press MUTE to"));
          display.setCursor(10, 47);
          display.println(F("clear the error."));
          display.display(); // Show initial text
          while (alertQueue[i].isCleared == 0)
          {
            // Serial.println("latched an error");
            delay(25);
            for (int e = 0; e < 50; e++)
            {
              if (alertQueue[e].isSilenced == 0)
              {
                // Serial.println("Move to Interupt Error Page");
                baseStationState = 4;
                break;
              }
            }
            if (baseStationState == 0 || baseStationState == 2 || baseStationState == 3 || baseStationState == 4)
            {
              break;
            }
            if (digitalRead(21) == LOW)
            {
              // latching debounce
              while (digitalRead(21) == LOW)
              {
                delay(10);
              }
              alertQueue[i].isCleared = 1;
              // Serial.println("changing error queued to 0");
              errorQueued = 0;
              lastInput = millis();
              // break;
            }
            // if (digitalRead(47) == LOW) {
            //   while(digitalRead(47) == LOW) {
            //     delay(10);
            //     }
            //     lastInput = millis();
            //     break;
            //   }
          }
        }
      } // else {
      if (errorQueued == 0)
      {
        // Serial.println("no errors queued");
        display.clearDisplay();
        display.setTextSize(2); // Draw 2X-scale text
        display.setCursor(1, 1);
        display.println(F("No Errors!"));
        display.setTextSize(1); // Draw 1X-scale text
        display.setCursor(10, 27);
        display.println(F("You've got some"));
        display.setCursor(10, 41);
        display.println(F("Happy Fishes."));
        display.display(); // Show initial text
        delay(25);
        break;
      }

    case 2:
// Current Sense Pagelong lastScreenSaver = millis();
#ifdef DEBUG_BaseStationState
      Serial.println("");
      Serial.println("Base Station State case 2 Current Sense Page");
      Serial.println("");
      delay(1000);
#endif
      if (millis() > lastCurrentTest + 500)
      {
        testCurrentSense();
        long lastCurrentTest = millis();
      }
      display.clearDisplay();
      display.setTextSize(2); // Draw 2X-scale text
      display.setCursor(1, 1);
      display.println(F("Current:"));
      display.setTextSize(1); // Draw 1X-scale text
      display.setCursor(10, 27);
      display.println(F("You are using:"));
      display.setCursor(16, 41);
      display.print(current_mA);
      display.print(" mA");
      display.display(); // Show initial text
      delay(25);
      break;

    case 3:
// Settings Page------------------------------------------------------------------------------------------------------------------------------
#ifdef DEBUG_BaseStationState
      Serial.println("");
      Serial.println("Base Station State case 3 Settings Page");
      Serial.println("");
      delay(1000);
#endif
      onlyOnce = 0;
      if (state3screen == 0)
      {
        display.clearDisplay();
        display.setTextSize(2); // Draw 2X-scale text
        display.setCursor(1, 1);
        display.println(F("Settings"));
        display.setTextSize(1); // Draw 1X-scale text
        display.setCursor(10, 17);
        display.print(F("Base Station ID:"));
        display.println(BASESTATION_ID);
        display.setCursor(10, 27);
        display.print(F("Current WiFi:"));
        display.setCursor(10, 37);
        display.println(WiFi.SSID());
        display.setCursor(10, 47);
        display.print(F("Hit MUTE for more"));
        display.setCursor(10, 57);
        display.println(F("options."));
        display.display(); // Show initial text
        while (state3screen == 0 && baseStationState == 3)
        {
          delay(10);
          for (int e = 0; e < 50; e++)
          {
            if (alertQueue[e].isSilenced == 0)
            {
              baseStationState = 3;
              break;
            }
          }
          if (baseStationState != 3)
          {
            break;
          }
          if (digitalRead(21) == LOW)
          {
            while (digitalRead(21) == LOW)
            {
              delay(10);
            }
            state3screen = 1;
            Serial.println("State 3 Screen 1");
            lastInput = millis();
          }
        }
      }
      if (state3screen == 1)
      {
        display.clearDisplay();
        display.setTextSize(2); // Draw 2X-scale text
        display.setCursor(1, 1);
        display.println(F("Settings:"));
        display.setTextSize(1); // Draw 1X-scale text
        display.setCursor(10, 17);
        display.print(F("Toggle CANBus"));
        display.setCursor(10, 27);
        display.print(F("Hold mute for 3"));
        display.setCursor(10, 37);
        display.println(F("seconds to toggle."));
        display.setCursor(10, 47);
        display.print(F("Tap MUTE for next"));
        display.setCursor(10, 57);
        display.println(F("setting."));
        display.display(); // Show initial text
        while (state3screen == 1 && baseStationState == 3)
        {
          delay(10);
          for (int e = 0; e < 50; e++)
          {
            if (alertQueue[e].isSilenced == 0)
            {
              baseStationState = 4;
              break;
            }
          }
          if (baseStationState != 3)
          {
            break;
          }
          if (digitalRead(21) == LOW)
          {
            buttPress = millis();
            while (digitalRead(21) == LOW)
            {
              delay(10);
              if (millis() > buttPress + 3000)
              {
                if (onlyOnce == 0)
                {
                  onlyOnce = 1;
                  annoyingBuzz();
                }
              }
              if (millis() > buttPress + 3000)
              {
                if (digitalRead(11) == HIGH)
                {
                  // The Action
                  Serial.println("CANBus Toggled");
                  screenSaverRunning = false;
                  baseStationState = 5;
                  MQTTdisconnect();
                  digitalWrite(11, LOW);
                }
              }
              else
              {
                state3screen = 2;
                Serial.println("State 3 Screen 2");
                lastInput = millis();
              }
            }
          }
        }
      }
      if (state3screen == 2)
      {
        display.clearDisplay();
        display.setTextSize(2); // Draw 2X-scale text
        display.setCursor(1, 1);
        display.println(F("Settings:"));
        display.setTextSize(1); // Draw 1X-scale text
        display.setCursor(10, 17);
        display.print(F("Buzzer Mute"));
        display.setCursor(10, 27);
        display.print(F("hold MUTE 3 seconds"));
        display.setCursor(10, 37);
        display.print(F("to toggle."));
        display.setCursor(10, 47);
        display.print(F("Tap MUTE for next"));
        display.setCursor(10, 57);
        display.println(F("option."));
        display.display(); // Show initial text
        while (state3screen == 2 && baseStationState == 3)
        {
          delay(10);
          for (int e = 0; e < 50; e++)
          {
            if (alertQueue[e].isSilenced == 0)
            {
              baseStationState = 4;
              break;
            }
          }
          if (baseStationState != 3)
          {
            break;
          }
          if (digitalRead(21) == LOW)
          {
            buttPress = millis();
            while (digitalRead(21) == LOW)
            {
              delay(10);
              if (millis() > buttPress + 3000)
              {
                if (onlyOnce == 0)
                {
                  onlyOnce = 1;
                  annoyingBuzz();
                }
              }
            }
            if (millis() > buttPress + 3000)
            {
              if (digitalRead(11) == HIGH)
              {
                // The Action
                if (antiBuzzer == 0)
                {
                  antiBuzzer = 1;
                  Serial.println("Buzzer Muted");
                }
                else
                {
                  antiBuzzer = 0;
                  Serial.println("Buzzer Unmuted");
                }
              }
            }
            else
            {
              state3screen = 3;
              Serial.println("State 3 Screen 3");
              lastInput = millis();
            }
          }
        }
      }
      if (state3screen == 3)
      {
        display.clearDisplay();
        display.setTextSize(2); // Draw 2X-scale text
        display.setCursor(1, 1);
        display.println(F("Settings:"));
        display.setTextSize(1); // Draw 1X-scale text
        display.setCursor(10, 17);
        display.print(F("Dark Mode"));
        display.setCursor(10, 27);
        display.print(F("Hold MUTE for 3"));
        display.setCursor(10, 37);
        display.print(F("seconds to toggle."));
        display.setCursor(10, 47);
        display.print(F("Tap MUTE for next"));
        display.setCursor(10, 57);
        display.println(F("option."));
        display.display(); // Show initial text
        while (state3screen == 3 && baseStationState == 3)
        {
          delay(10);
          for (int e = 0; e < 50; e++)
          {
            if (alertQueue[e].isSilenced == 0)
            {
              baseStationState = 4;
              break;
            }
          }
          if (baseStationState != 3)
          {
            break;
          }
          if (digitalRead(21) == LOW)
          {
            buttPress = millis();
            Serial.println("Dark Mode Button Pressed");
            while (digitalRead(21) == LOW)
            {
              delay(10);
              if (millis() > buttPress + 3000)
              {
                if (onlyOnce == 0)
                {
                  onlyOnce = 1;
                  annoyingBuzz();
                }
              }
            }
            if (millis() > buttPress + 3000)
            {
              // The Action
              // Set the nightMode flag opposite of what it is
              if (nightMode == 0)
              {
                nightMode = 1;
                Serial.println("Night Mode On");
              }
              else
              {
                nightMode = 0;
                Serial.println("Night Mode Off");
              }
            }
            else
            {
              state3screen = 4;
              Serial.println("State 3 Screen 4");
              lastInput = millis();
            }
          }
        }
      }
      if (state3screen == 4)
      {
        display.clearDisplay();
        display.setTextSize(2); // Draw 2X-scale text
        display.setCursor(1, 1);
        display.println(F("Settings:"));
        display.setTextSize(1); // Draw 1X-scale text
        display.setCursor(10, 17);
        display.print(F("RESTART SYSTEM"));
        display.setCursor(10, 27);
        display.print(F("Hold MUTE for 5"));
        display.setCursor(10, 37);
        display.print(F("seconds to restart."));
        display.setCursor(10, 47);
        display.print(F("Tap MUTE for next"));
        display.setCursor(10, 57);
        display.println(F("option."));
        display.display(); // Show initial text
        while (state3screen == 4 && baseStationState == 3)
        {
          delay(10);
          for (int e = 0; e < 50; e++)
          {
            if (alertQueue[e].isSilenced == 0)
            {
              baseStationState = 4;
              break;
            }
          }
          if (baseStationState != 3)
          {
            break;
          }
          if (digitalRead(21) == LOW)
          {
            buttPress = millis();
            while (digitalRead(21) == LOW)
            {
              delay(10);
              if (millis() > buttPress + 5000)
              {
                if (onlyOnce == 0)
                {
                  onlyOnce = 1;
                  annoyingBuzz();
                }
              }
            }
            if (millis() > buttPress + 5000)
            {
              // The Action
              // Shut down the whole ESP32
              Serial.println("Restarting System");
              delay(1000);
              ESP.restart();
            }
            else
            {
              state3screen = 0;
              Serial.println("State 3 Screen 0");
              lastInput = millis();
            }
          }
        }
      }
      // Base Station Utilities
      delay(25);
      break;

    case 4:
// Error Warning Screen
#ifdef DEBUG_BaseStationState
      Serial.println("");
      Serial.println("Base Station State case 4 Error Warning Screen");
      Serial.println("");
      delay(1000);
#endif
      for (int i = 0; i < 50; i++)
      {
        if (alertQueue[i].isSilenced == 0)
        {
          errorQueued = 1;
          // Serial.println("Interupt Error Page");
          display.clearDisplay();
          display.setTextSize(2); // Draw 2X-scale text
          display.setTextColor(SSD1306_WHITE);
          display.setCursor(1, 1);
          display.print(F("!!Error "));
          display.print(i + 1);
          display.println(F(":"));
          display.setTextSize(1); // Draw 1X-scale text
          display.setCursor(10, 17);
          display.println(F("Error on node"));
          display.setCursor(10, 27);

          String deviceName = "Node " + String(alertQueue[i].nodeID);

          // Send manifest data to the MQTT broker
          if (LittleFS.exists(manifestFileName))
          {
            Serial.println("Manifest file exists");
            File manifestFile = LittleFS.open(manifestFileName, FILE_READ);
            String manifestString = manifestFile.readString();
            manifestFile.close();

            JsonDocument manifestDoc;

            // Parse the JSON object
            DeserializationError error = deserializeJson(manifestDoc, manifestString);

            if (error)
            {
              Serial.println("Error parsing manifest file");
            }
            else
            {
              // Get the devices array from the manifest file
              JsonArray devices = manifestDoc["Devices"].as<JsonArray>();
              uint16_t index = 0;
              while (!devices[index].isNull())
              {
                Serial.println("Checking device: " + devices[index]["NodeID"].as<String>());
                JsonObject device = devices[index];
                if (device["NodeID"] == alertQueue[i].nodeID)
                {
                  Serial.println("Device found in manifest file");
                  deviceName = device["DeviceName"].as<String>();
                  break;
                }
                index++;
              }
            }
          }
          else
          {
            Serial.println("Manifest file does not exist");
          }

          display.println(deviceName);
          // display.println(alertQueue[i].nodeID);
          display.setCursor(10, 37);
          display.println(F("Press MUTE to "));
          display.setCursor(10, 47);
          display.println(F("silence the alert."));
          display.display(); // Show initial text
          delay(10);
          while (alertQueue[i].isSilenced == 0)
          {
            annoyingBuzz();
            delay(50);
            if (digitalRead(21) == LOW)
            {
              delay(25);
              // latching debounce
              while (digitalRead(21) == LOW)
              {
                delay(10);
              }
              alertQueue[i].isSilenced = 1;
              break;
            }
          }
        }
        delay(25);
      }
      // Serial.println("Exiting Interupt Screen");
      baseStationState = 1;
      break;

    case 5:
// CANBus Offline Screen
#ifdef DEBUG_BaseStationState
      Serial.println("");
      Serial.println("Base Station State case 5 CANBus Offline Screen");
      Serial.println("");
      delay(1000);
#endif
      onlyOnce = 0;
      // CANBus OFF
      display.clearDisplay();
      display.setTextSize(2); // Draw 2X-scale text
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(10, 1);
      display.println(F("CANBus"));
      display.setCursor(10, 17);
      display.println(F("Offline"));
      display.setTextSize(1); // Draw 1X-scale text
      display.setCursor(10, 47);
      display.println(F("Hold MUTE to"));
      display.setCursor(10, 57);
      display.println(F("reconnect."));
      display.display(); // Show initial text
      delay(25);
      while (baseStationState == 5)
      {
        delay(25);
        if (digitalRead(21) == LOW)
        {
          buttPress = millis();
          while (digitalRead(21) == LOW)
          {
            delay(10);
            if (millis() > buttPress + 5000)
            {
              if (onlyOnce == 0)
              {
                onlyOnce = 1;
                annoyingBuzz();
              }
            }
          }
          if (millis() > buttPress + 5000)
          {
            if (digitalRead(11) == LOW)
            {
              Serial.println("CANBus On");
              screenSaverRunning = false;
              baseStationState = 0;
              xTaskCreatePinnedToCore(
                  mqttLoop,   /* Function to implement the task */
                  "mqttLoop", /* Name of the task */
                  10000,      /* Stack size in words */
                  NULL,       /* Task input parameter */
                  1,          /* Priority of the task */
                  &xMQTTloop, /* Task handle. */
                  0);         /* Core where the task should run */
            }
          }
          lastInput = millis();
        }
      }
      break;

    default:
      delay(25);
      break;
    }
  }
  Serial.println("");
}

// NeoPixel Task
void neoPixelTask(void *parameters)
{
  Serial.println("Running void neoPixelTask(void *parameters)");
  long lightingLoop;
  bool lightingLoopState;
  while (1)
  {
    // if there is an error in the UIAlertData, display the error message
    // switch case to control the neopixel's
    switch (baseStationState)
    {
    case 0:
      // Screen Saver breathing
      if (nightMode == 0) //
      {
        for (int i = 0; i < 150; i++)
        {
          if (baseStationState != 0)
          {
            break;
          }
          pixels.clear();
          pixels.setPixelColor(0, pixels.Color((150 - (i)), 0, 0));
          pixels.setPixelColor(1, pixels.Color(0, 0, 0));
          pixels.setBrightness(LEDBrightness);
          pixels.show();
          delay(25);
        }
        for (int i = 0; i < 150; i++)
        {
          if (baseStationState != 0)
          {
            break;
          }
          pixels.clear();
          pixels.setPixelColor(0, pixels.Color((0 + (i)), 0, 0));
          pixels.setPixelColor(1, pixels.Color(0, 0, 0));
          pixels.setBrightness(LEDBrightness);
          pixels.show();
          delay(25);
        }
      }
      else
      {
        delay(25);
      }
      break;

    case 1:
      // Error Lighting
      if (errorQueued == 1)
      {
        if (millis() >= lightingLoop + 1500)
        {
          lightingLoop = millis();
          if (lightingLoopState == 0)
          {
            pixels.clear();
            pixels.setPixelColor(0, pixels.Color(10, 0, 0));
            pixels.setPixelColor(1, pixels.Color(10, 10, 0));
            pixels.setBrightness(LEDBrightness);
            pixels.show(); // Send the updated pixel colors to the hardware.
            lightingLoopState = 1;
            delay(25); // Pause before next pass through loop
          }
          else
          {
            pixels.clear();
            pixels.setPixelColor(0, pixels.Color(10, 0, 0));
            pixels.setPixelColor(1, pixels.Color(0, 0, 0));
            pixels.setBrightness(LEDBrightness);
            pixels.show(); // Send the updated pixel colors to the hardware.
            lightingLoopState = 0;
            delay(25); // Pause before next pass through loop
          }
        }
        break;
      }
      else
      {
        pixels.clear();
        pixels.setPixelColor(0, pixels.Color(10, 0, 0));
        pixels.setPixelColor(1, pixels.Color(10, 0, 0));
        pixels.setBrightness(LEDBrightness);
        pixels.show(); // Send the updated pixel colors to the hardware.
        delay(25);
        break;
      }

    case 2:
      // Current Sense Lighting
      if (millis() >= lightingLoop + 1500)
      {
        lightingLoop = millis();
        if (lightingLoopState == 0)
        {
          pixels.clear();
          pixels.setPixelColor(0, pixels.Color(10, 0, 0));
          pixels.setPixelColor(1, pixels.Color(0, 0, 10));
          pixels.setBrightness(LEDBrightness);
          pixels.show(); // Send the updated pixel colors to the hardware.
          lightingLoopState = 1;
          delay(25); // Pause before next pass through loop
        }
        else
        {
          pixels.clear();
          pixels.setPixelColor(0, pixels.Color(10, 0, 0));
          pixels.setPixelColor(1, pixels.Color(0, 0, 0));
          pixels.setBrightness(LEDBrightness);
          pixels.show(); // Send the updated pixel colors to the hardware.
          lightingLoopState = 0;
          delay(25); // Pause before next pass through loop
        }
      }
      else
      {
        delay(25);
      }
      break;

    case 3:
      // Settings Lighting
      if (millis() >= lightingLoop + 1500)
      {
        lightingLoop = millis();
        if (lightingLoopState == 0)
        {
          pixels.clear();
          pixels.setPixelColor(0, pixels.Color(10, 0, 0));
          pixels.setPixelColor(1, pixels.Color(10, 0, 10));
          pixels.setBrightness(LEDBrightness);
          pixels.show(); // Send the updated pixel colors to the hardware.
          lightingLoopState = 1;
          delay(25); // Pause before next pass through loop
        }
        else
        {
          pixels.clear();
          pixels.setPixelColor(0, pixels.Color(10, 0, 0));
          pixels.setPixelColor(1, pixels.Color(0, 0, 0));
          pixels.setBrightness(LEDBrightness);
          pixels.show(); // Send the updated pixel colors to the hardware.
          lightingLoopState = 0;
          delay(25); // Pause before next pass through loop
        }
      }
      else
      {
        delay(25);
      }
      break;

    case 4:
      // Warning Lighting
      if (millis() >= lightingLoop + 1000)
      {
        lightingLoop = millis();
        if (lightingLoopState == 0)
        {
          pixels.clear();
          pixels.setPixelColor(0, pixels.Color(0, 255, 0));
          pixels.setPixelColor(1, pixels.Color(255, 255, 255));
          pixels.setBrightness(LEDBrightness);
          pixels.show(); // Send the updated pixel colors to the hardware.
          lightingLoopState = 1;
          delay(25);
        }
        else
        {
          pixels.clear();
          pixels.setPixelColor(0, pixels.Color(255, 255, 255));
          pixels.setPixelColor(1, pixels.Color(0, 255, 0));
          pixels.setBrightness(LEDBrightness);
          pixels.show(); // Send the updated pixel colors to the hardware.
          delay(25);
          lightingLoopState = 0;
        }
      }
      else
      {
        delay(25);
      }
      break;

    case 5:
      // CANBus OFF
      if (millis() >= lightingLoop + 1000)
      {
        lightingLoop = millis();
        if (lightingLoopState == 0)
        {
          pixels.clear();
          pixels.setPixelColor(0, pixels.Color(0, 10, 10));
          pixels.setPixelColor(1, pixels.Color(0, 10, 10));
          pixels.setBrightness(LEDBrightness);
          pixels.show(); // Send the updated pixel colors to the hardware.
          lightingLoopState = 1;
          delay(25);
        }
        else
        {
          pixels.clear();
          pixels.setPixelColor(0, pixels.Color(0, 0, 0));
          pixels.setPixelColor(1, pixels.Color(0, 0, 0));
          pixels.setBrightness(LEDBrightness);
          pixels.show(); // Send the updated pixel colors to the hardware.
          delay(25);
          lightingLoopState = 0;
        }
      }
      else
      {
        delay(25);
      }
      break;

    default:
      delay(100);
      break;
    }
  }
  Serial.println("");
}

// MQTT Task
void mqttLoop(void *_this)
{
  Serial.println("Running void mqttLoop(void *_this)");
  Serial.println("Starting MQTT Loop");
  while (1)
  {
    // If the MQTT is not connected, or has disconnect4ed for some reason, connect to the MQTT broker
    if (!mqttClient.connected())
    {
      // digitalWrite(11, LOW);
      MQTTConnect();
      delay(100);
    }
    mqttClient.loop();
    delay(10);
  }
  Serial.println("");
}

// Function to Update the RTC
void updateRTC(void *parameters)
{
  while (1)
  {
    //Serial.println("Running void updateRTC(void *parameters)");
    delay(1000);
    // Time--------------------------------------------------------------------------------------

    // Globals for time
    // struct tm timeinfo;             // Time struct
    // char localTime[64];             // Local time string
    // String localTimeZone = "UTC+8"; // Default timezone is Pacific Time

    /*
     * @brief Represents calendar time.
     *
     * The time_t type is used to represent the number of seconds elapsed since
     * 00:00 hours, Jan 1, 1970 UTC (i.e., Unix epoch). It is typically used for
     * time-related functions and operations.
     */

    uint64_t unixTimeData;
    time_t UNIXtime = time(NULL);
    unixTimeData = (uint64_t)UNIXtime; //  Gets the unix time from the ntp server and stores it in the now variable
    //localTimeZone = ("UTC+" + String(localTimeZoneOffset));
    //localTimeZone.toCharArray(buf_localTimeZone, 8);
    //setenv("TZ", buf_localTimeZone, 1);
    //tzset();
    localtime_r(&UNIXtime, &timeinfo);
    strftime(localTime, sizeof(localTime), "%c", &timeinfo);

#ifdef DEBUGTime
    Serial.println("Get UNIX time from NTP server =");
    Serial.println("time_t UNIXtime = " + String(UNIXtime));
    Serial.println("");
    Serial.println("New RTC Update");
    Serial.println("unixTimeData = UNIXtime = " + String(unixTimeData));
    Serial.println(String(timeinfo.tm_year + 1900) + "-" + String(timeinfo.tm_mon + 1) + "-" + String(timeinfo.tm_mday) + " " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec));
    String CtimeNow = ctime(&UNIXtime);
    Serial.println(String(CtimeNow));
    Serial.println("time_t UNIX timestamp now = " + String(UNIXtime));
    Serial.println("Serial.println(&timeinfo) = " + String(localTime));
    Serial.println("localTimeZone = " + String(localTimeZone));
    Serial.println("TZ = " + String(buf_localTimeZone));
    Serial.println("timeZoneOffsetData = localTimeZoneOffset = " + String(timeZoneOffsetData));
    Serial.println("localTime = " + String(localTime));
    Serial.println("localTimeZone = " + String(localTimeZone));
    Serial.println("localTimeZoneOffset = " + String(localTimeZoneOffset));
    Serial.println("");
#endif
    if (millis() > lastRTCUpdate + 10000)
    {
      twai_message_t UNIXtimeMessage = core.create_message(UPDATE_UNIX_TIMESTAMP_MESSAGE_ID, NODE_ID, &unixTimeData);
      if (twai_transmit(&UNIXtimeMessage, 2000) == ESP_OK)
      {
        Serial.println("Sending UNIX Time Message, queued for transmission");
        Serial.println("unixTimeData = " + String(unixTimeData));
        Serial.println("");
      }
      else
      {
        Serial.println("Failed to Sending UNIX Time Message");
        Serial.println("");
      }
      sendTimezoneOffsetMessage(localTimeZoneOffset);
      lastRTCUpdate = millis();
    }
  }
}

void SendMQTTMessage(uint8_t _systemID, uint8_t _baseStationID, uint8_t _nodeID, uint16_t _messageID, uint64_t _data)
{
  delay(100);
  Serial.println("Running void SendMQTTMessage(uint8_t _systemID, uint8_t _baseStationID, uint8_t _nodeID, uint16_t _messageID, uint64_t _data)");
  // Create the MQTT topic string
  String topic = "out/" + String(_systemID) + "/" + String(_baseStationID) + "/" + String(_nodeID) + "/" + String(_messageID);

  // Allocate the JSON document
  JsonDocument doc;

  // Add the current time and the data from the CAN Bus message to the JSON doc
  time_t now;
  time(&now);
  doc["time"] = now;
  doc["data"] = _data;

  // Convert the JSON Doc to text
  String payload = String(doc.as<String>());

  // Send the message to the MQTT broker
  mqttClient.publish(topic.c_str(), payload.c_str(), true);
  Serial.println("mqttClient.publish(topic.c_str(), payload.c_str(), true)");
  Serial.println("topic = " + String(topic));
  Serial.println("payload = " + String(payload));
  Serial.println("");
}
