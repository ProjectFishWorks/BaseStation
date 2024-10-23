#include "dataLogging.h"

//Initialize the SD card
void initSDCard() {
    //Mount the SD Card
    SPI.begin(sd_sck, sd_miso, sd_mosi, sd_cs);
    if (!SD.begin(sd_cs)) {
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

    //TODO: For when we get the log file queue working
    //log_file_queue = xQueueCreate(LOG_FILE_QUEUE_LENGTH, sizeof(String));
    //xTaskCreate(writeLogFileQueueTask, "writeLogFileQueueTask", 10000, &log_file_queue, 1, NULL);
}

//Write log data to the SD card, creating a new log file if needed
void writeLogData(uint16_t systemID, uint16_t baseStationID, String baseStationFirmwareVersion, uint8_t nodeID, uint16_t messageID, uint64_t data) {
  //Create the log data row
  String logData = getLogDataRow(nodeID, messageID, data);
  File logFile;
  char filename[255];
  getCurrentLogFilename(filename, systemID, baseStationID);
  // //TODO: For when we get the log file queue working, for now just write directly to the file
  // //xQueueSend(log_file_queue, &logData, portMAX_DELAY);
  openLogFile(&logFile, systemID, baseStationID, String(baseStationFirmwareVersion));
  logFile.println(logData);
  // Serial.println("Wrote to log file : " + logData);
  logFile.close();
}

//Send log data to the MQTT broker
void sendLogData(uint16_t systemID, uint16_t baseStationID, uint8_t nodeID, uint16_t messageID, uint16_t hoursToRead, PubSubClient* client){

  //Allocate the JSON document
  JsonDocument doc;
  //Read the log data from the SD card
  
  time_t now;
  time(&now);
  uint64_t intervalStartTime = now - hoursToRead * 3600;
  doc.clear();

 time_t currentHour = now;

  while((currentHour) + 1 > intervalStartTime){
    readLogData(systemID, baseStationID, nodeID, messageID, currentHour, intervalStartTime, &doc);
    //Create the MQTT topic string
    //Create the time portion of the topic
    struct tm hour;
    localtime_r(&currentHour, &hour);

    char timeString[255];
    //Format the data/time
    strftime(timeString, 255, "%Y%m%d%H", &hour);

    String topic = "historyOut/" + String(systemID) + "/" + String(baseStationID) + "/" + String(nodeID) + "/" + String(messageID) + "/" + timeString;
    Serial.println("Sending log data to MQTT topic: " + topic);
    client->publish(topic.c_str(), doc.as<String>().c_str());

    client->loop();

    doc.clear();
    currentHour -= 3600;
  }
  
}

//Read log data from the SD card for a specific hour, also ensuring the data is after a specific start time to allow for partial hour reads
uint8_t readLogData(uint16_t systemID, uint16_t baseStationID, uint8_t nodeID, uint16_t messageID, uint64_t hourToRead, uint64_t intervalStartTime, JsonDocument *doc) {
  
  //Add some metadata to the JSON doc
  (*doc)["hour"] = hourToRead;
  (*doc)["systemID"] = systemID;
  (*doc)["baseStationID"] = baseStationID;
  (*doc)["nodeID"] = nodeID;
  (*doc)["messageID"] = messageID;

  JsonArray history = (*doc)["history"].to<JsonArray>();
    //Read historical log data from the SD card
    char filename[255];

    time_t now;

    now = hourToRead;

    char field[MAX_LOG_FILE_FIELD_LENGTH];

    File logFile;

    //Get the filename for the log file
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    getLogFilename(filename, systemID, baseStationID, timeinfo);

    if(SD.exists(filename)) {

      //Open the log file
      logFile = SD.open(filename, FILE_READ);
      Serial.println("Opened log file: " + String(filename));
      //Skip the header rows
      for(uint8_t i = 0; i < LOG_FILE_HEADER_ROW_COUNT; i++){
        logFile.readStringUntil('\n');
      }
      
      //Row counter to limit the number of rows read to fit within the MQTT message buffer size
      uint16_t rowCounter = 0;
      while(logFile.available()){
        uint64_t dataTime = 0;
        uint32_t dataNodeID = 0;
        uint32_t dataMessageID = 0;
        uint64_t data = 0;

        uint8_t fieldLength = 0;

        //Parse the expected fields from the log file row
        parseLogDataField(&logFile, field, MAX_LOG_FILE_FIELD_LENGTH);
        dataTime = strtoull(field, NULL, 10);
        parseLogDataField(&logFile, field, MAX_LOG_FILE_FIELD_LENGTH);
        dataNodeID = strtoul(field, NULL, 10);
        parseLogDataField(&logFile, field, MAX_LOG_FILE_FIELD_LENGTH);
        dataMessageID = strtoul(field, NULL, 10);
        parseLogDataField(&logFile, field, MAX_LOG_FILE_FIELD_LENGTH);
        data = strtoull(field, NULL, 10);

        //If the data matches the node and message ID, and the time is within the interval, add it to the JSON doc
        if(dataNodeID == nodeID && dataMessageID == messageID && dataTime >= intervalStartTime && rowCounter < 500){
          JsonObject nestedObject = history.createNestedObject();
          nestedObject["time"] = dataTime;
          nestedObject["data"] = data;
          rowCounter++;
        }

      }
    }
    else{
      Serial.println("Log file does not exist, skipping: " + String(filename));
    }
  return 1;

}

//Parse a field from a log csv data row
uint8_t parseLogDataField(File *file, char* field, size_t maxFieldLength) {
  size_t n = 0;
  char ch;
  while((n+1) < maxFieldLength && file->readBytes(&ch,1) == 1) {
    //Skip the carriage return
    if(ch == '\r'){
      continue;
    }
    field[n] = ch;

    //If we reach a comma or newline, end the field
    if(ch == ',' || ch == '\n'){
      //Null terminate the string
      field[n] = '\0';
      return n;
    }
    n++;
  }
}

void getLogFilename(char* filename, uint16_t systemID, uint16_t baseStationID, tm timeinfo){

  //Buffer to hold the current date/time
  char timeString[255];
  //Format the data/time
  strftime(timeString, 255, "-%Y-%m-%d-%H.csv", &timeinfo);

  //Combine the date/time with system and base station ID
  String filenameString = "/log-" + String(systemID) + "-" + String(baseStationID) + timeString;;

  //Copy the final string to the filename pointer
  filenameString.toCharArray(filename, filenameString.length() + 1);

}
//Get the current log file name based on the current time
//Currently creates one log file per hour
void getCurrentLogFilename(char* filename, uint16_t systemID, uint16_t baseStationID){
  //Time object
  struct tm timeinfo;
  //If we can't get the current time, add a default filename
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time!");
    strcpy(filename, "log-no-time.csv");
    return;
  }

  getLogFilename(filename, systemID, baseStationID, timeinfo);
  
}

//Write the header info to the log file
void writeLogHeader(File *file, uint16_t systemID, uint16_t baseStationID, String baseStationFirmwareVersion) {
  file->println(("SystemID," + String(systemID)));
  file->println("BaseStationID," + String(baseStationID));
  file->println("BaseStationFirmwareVersion," + baseStationFirmwareVersion);

  time_t now;
  time(&now);
  file->println("Created," + String(now));
  file->println("Time,NodeID,MessageID,Data");
}

//Creates a row of data for the log file
String getLogDataRow(uint8_t nodeID, uint16_t messageID, uint64_t data) {
  time_t now;
  time(&now);
  return String(now) + "," + String(nodeID) + "," + String(messageID) + "," + String(data);
}

//Open the log file and creates a new one if it doesn't exist
void openLogFile(File *file, uint16_t systemID, uint16_t baseStationID, String baseStationFirmwareVersion) {
  //Get the current filename based on the current time
  char filename[255];
  getCurrentLogFilename(filename, systemID, baseStationID);

  //If the file exists, open it in append mode, otherwise create a new file
  if(SD.exists(filename)) {
    *file = SD.open(filename, FILE_APPEND);
  } else {
    Serial.println("Log file does not exist, creating new file with filename: " + String(filename));
    *file = SD.open(filename, FILE_WRITE);
    writeLogHeader(file, systemID, baseStationID, baseStationFirmwareVersion);
  }
}

//Not working right now, TODO fix this shit
/* void  writeLogFileQueueTask(void *queue) {
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
} */

