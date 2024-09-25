#include "dataLogging.h"

void initSDCard() {
    //Mount the SD Card
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

    //TODO: For when we get the log file queue working
    //log_file_queue = xQueueCreate(LOG_FILE_QUEUE_LENGTH, sizeof(String));
    //xTaskCreate(writeLogFileQueueTask, "writeLogFileQueueTask", 10000, &log_file_queue, 1, NULL);
}

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

uint8_t readLogData(uint16_t systemID, uint16_t baseStationID, uint8_t nodeID, uint16_t messageID, uint16_t hoursToRead, JsonDocument *doc) {
  
  
  (*doc)["hours"] = hoursToRead;
  (*doc)["systemID"] = systemID;
  (*doc)["baseStationID"] = baseStationID;
  (*doc)["nodeID"] = nodeID;
  (*doc)["messageID"] = messageID;

  JsonArray history = (*doc)["history"].to<JsonArray>();
  for(uint16_t i = 0; i < hoursToRead; i++){

    //Read historical log data from the SD card
    char filename[255];

    time_t now;
    time(&now);

    now -= i * 3600;

    char field[MAX_LOG_FILE_FIELD_LENGTH];

    File logFile;

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    getLogFilename(filename, systemID, baseStationID, timeinfo);

    if(SD.exists(filename)) {
      logFile = SD.open(filename, FILE_READ);
      Serial.println("Opened log file: " + String(filename));
      //Skip the header rows
      for(uint8_t i = 0; i < LOG_FILE_HEADER_ROW_COUNT; i++){
        logFile.readStringUntil('\n');
      }

      while(logFile.available()){
        uint64_t time = 0;
        uint32_t nodeID = 0;
        uint32_t messageID = 0;
        uint64_t data = 0;
        uint8_t fieldLength = 0;

        parseLogDataField(&logFile, field, MAX_LOG_FILE_FIELD_LENGTH);
        time = strtoull(field, NULL, 10);
        parseLogDataField(&logFile, field, MAX_LOG_FILE_FIELD_LENGTH);
        nodeID = strtoul(field, NULL, 10);
        parseLogDataField(&logFile, field, MAX_LOG_FILE_FIELD_LENGTH);
        messageID = strtoul(field, NULL, 10);
        parseLogDataField(&logFile, field, MAX_LOG_FILE_FIELD_LENGTH);
        data = strtoull(field, NULL, 10);

        if(nodeID == nodeID && messageID == messageID){
          JsonObject nestedObject = history.createNestedObject();
          nestedObject["time"] = time;
          nestedObject["data"] = data;
        }

      }
    }
    else{
      Serial.println("Log file does not exist, skipping: " + String(filename));
    }
  }
  return 1;

}

uint8_t parseLogDataField(File *file, char* field, size_t maxFieldLength) {
  size_t n = 0;
  char ch;
  while((n+1) < maxFieldLength && file->readBytes(&ch,1) == 1) {
    //Skip the carriage return
    if(ch == '\r'){
      continue;
    }
    field[n] = ch;
    if(ch == ',' || ch == '\n'){
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

