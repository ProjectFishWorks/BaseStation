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
#define baseStationID 0x02

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


//NeoPixel Setup Stuffs
#define PIN         45 // On Trinket or Gemma, suggest changing this to 1
// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS   2 // Popular NeoPixel ring size
// When setting up the NeoPixel library, we tell it how many pixels,
// and which pin to use to send signals. Note that for older NeoPixel
// strips you might need to change the third parameter -- see the
// strandtest example for more information on possible values.
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#define DELAYVAL    500 // Time (in milliseconds) to pause between pixels


//LCD Screen Setup Stuffs
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

int I2C_SDA = 18;
int I2C_SCL = 19;


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES     10 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16
static const unsigned char PROGMEM logo_bmp[] =
{ 0b00000000, 0b11000000,
  0b00000001, 0b11000000,
  0b00000001, 0b11000000,
  0b00000011, 0b11100000,
  0b11110011, 0b11100000,
  0b11111110, 0b11111000,
  0b01111110, 0b11111111,
  0b00110011, 0b10011111,
  0b00011111, 0b11111100,
  0b00001101, 0b01110000,
  0b00011011, 0b10100000,
  0b00111111, 0b11100000,
  0b00111111, 0b11110000,
  0b01111100, 0b11110000,
  0b01110000, 0b01110000,
  0b00000000, 0b00110000 };


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

    mqttClient.loop();



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

void backlightToggle() {
  Serial.println("Toggling Backlight");
  if (digitalRead(3) == HIGH) {
    digitalWrite(3, LOW);
  } else {
    digitalWrite(3, HIGH);
  }
  if (digitalRead(3) == HIGH) {
    Serial.println("Backlight On");
  } else {
    Serial.println("Backlight Off");
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
    (display.width()  - LOGO_WIDTH ) / 2,
    (display.height() - LOGO_HEIGHT) / 2,
    logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
  display.display();
  delay(1000);
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

  for(;;) { // Loop forever...
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

void testLCDScreen () {
  testdrawbitmap();    // Draw a small bitmap image
  Serial.println("Test draw bitmap");
  // Invert and restore display, pausing in-between

  delay(1000);
  Serial.println("Success");
  testanimate(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT); // Animate bitmaps
  Serial.println("Test animate");
}
//Yeah, There is lots of it.

void setup() {
    //Start the serial connection
    Serial.begin(115200);

    pinMode(0, INPUT_PULLUP);  //Program button with pullup
    pinMode(21, INPUT_PULLUP); //ButtBlue
    pinMode(47, INPUT_PULLUP); //ButtWhite
    pinMode(48, OUTPUT); //Buzzer

    pinMode(11, OUTPUT); //CAN Bus Power

    
    Wire.begin(I2C_SDA, I2C_SCL);
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      Serial.println(F("SSD1306 allocation failed"));
      for(;;); // Don't proceed, loop forever
      }

    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.
    display.display();
    delay(1000); // Pause for 1 seconds




    Serial.println(F("Initialized LCD Screen"));

    if (!LittleFS.begin(0)) {
      Serial.println("LittleFS Mount Failed");
      //TODO: stall startup here
      return;
    }

    Serial.println("LittleFS Mount Successful");

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
    //wm.resetSettings();

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
      
/*       display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(4,4);
      display.println(WiFi.softAPIP());
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.println("123456789012345");
      display.setCursor(0, 28);
      display.println("123456789012345");
      display.setCursor(0, 36);
      display.println("123456789012345678901234567890");
      display.display(); */
      display.clearDisplay();

      display.setTextSize(2); // Draw 2X-scale text
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(10, 0);
      display.println(F("scroll"));
      display.display();      // Show initial text
      delay(100);

      // Scroll in various directions, pausing in-between:
      display.startscrollright(0x00, 0x0F);
      delay(2000);
      display.stopscroll();
      delay(1000);
      display.startscrollleft(0x00, 0x0F);
      delay(2000);
      display.stopscroll();
      delay(1000);
      display.startscrolldiagright(0x00, 0x07);
      delay(2000);
      display.startscrolldiagleft(0x00, 0x07);
      delay(2000);
      display.stopscroll();
      delay(1000);

    });

    res = wm.autoConnect("Project Fish Works Base Station");
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
    espClient.setCACert(CA_cert);  

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

    digitalWrite(11, HIGH); //Turn on the CAN Bus power
    digitalWrite(48, LOW); //Turn off the buzzer
    digitalWrite(3, HIGH); //Turn on the LCD Backlight
    digitalWrite(19, HIGH); //Keep LCD on

    //NeoPixel Setup Stuffs part 2
    #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
    #endif
    pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)


    //LCD Screen Setup Stuffs part 2  

    

}

void loop() {

    //TODO: Move the MQTT connection to a separate task
    //If the MQTT is not connected, or has disconnect4ed for some reason, connect to the MQTT broker
    if (!mqttClient.connected()) {
        MQTTConnect();
    }
    mqttClient.loop();

    emailClientLoop(email_recipient, email_recipient_name);

    if (digitalRead(0) == LOW) {
      backlightToggle();
      //latching debounce
      while(digitalRead(0) == LOW){
        delay(50);
        }
      }
    if (digitalRead(21) == LOW) {
      Serial.println("Button 21 pressed");
      //annoyingBuzz();
      testLCDScreen();
      //latching debounce
      while(digitalRead(21) == LOW){
        delay(50);
        }
      }
    if (digitalRead(47) == LOW) {
    Serial.println("Button 47 pressed");
      //laching debounce
      while(digitalRead(47) == LOW){
        pixels.clear(); // Set all pixel colors to 'off'

          // The first NeoPixel in a strand is #0, second is 1, all the way up
          // to the count of pixels minus one.
          for(int i=0; i<NUMPIXELS; i++) { // For each pixel...

            // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
            // Here we're using a moderately bright green color:
            pixels.setPixelColor(i, pixels.Color(0, 150, 0));

            pixels.show();   // Send the updated pixel colors to the hardware.

            delay(DELAYVAL); // Pause before next pass through loop
        delay(50);
      }
    }
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.setPixelColor(1, pixels.Color(0, 0, 0));
    pixels.show(); // Set all pixel colors to 'off'
    }
}