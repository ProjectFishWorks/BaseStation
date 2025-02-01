#ifndef dataLogging_h
#define dataLogging_h

#include <SPI.h>

#include <SD.h>

#include <ArduinoJson.h>

#include <PubSubClient.h>

//TODO: this can probably be shorter
#define MAX_LOG_FILE_FIELD_LENGTH 30

#define LOG_FILE_HEADER_ROW_COUNT 6

//SD Card Pins
#define sd_sck 7
#define sd_miso 6
#define sd_mosi 15
#define sd_cs 16

//INITIALIZE SD CARD
void initSDCard();

//Write log data to the SD card, creating a new log file if needed
void writeLogData(uint16_t systemID, uint16_t baseStationID, String baseStationFirmwareVersion, uint8_t nodeID, uint16_t messageID, uint64_t data);

uint8_t readLogData(uint16_t systemID, uint16_t baseStationID, uint8_t nodeID, uint16_t messageID, uint64_t hourToRead, uint64_t intervalStartTime, JsonDocument *doc, uint32_t decimationInterval);

uint8_t parseLogDataField(File *file, char* field, size_t maxFieldLength);

//Get the log file name based on the systemID, baseStationID, and timeinfo
void getLogFilename(char* filename, uint16_t systemID, uint16_t baseStationID, uint16_t messageID, tm timeinfo);

//Get the current log file name based on the current time
void getCurrentLogFilename(char* filename, uint16_t systemID, uint16_t baseStationID, uint16_t messageID);

void sendLogData(String userID, uint16_t systemID, uint16_t baseStationID, uint8_t nodeID, uint16_t messageID, uint16_t hoursToRead, PubSubClient* client);

//Write the log file header
void writeLogHeader(File *file, uint16_t systemID, uint16_t baseStationID, uint16_t messageID, String baseStationFirmwareVersion);

//Get string for a log data row based on the nodeID, messageID, and data
String getLogDataRow(uint8_t nodeID, uint16_t messageID, uint64_t data);

//Open the log file for writing, creating a new log file if needed
void openLogFile(File *file, char* filename, uint16_t systemID, uint16_t baseStationID, uint16_t messageID, String baseStationFirmwareVersion, tm timeinfo);

#endif