#ifndef dataLogging_h
#define dataLogging_h

#include <SPI.h>

#include <SD.h>

#include <ArduinoJson.h>

//TODO: this can probably be shorter
#define MAX_LOG_FILE_LINE_LENGTH 255

#define LOG_FILE_HEADER_ROW_COUNT 5

//SD Card Pins
#define sck 3
#define miso 2
#define mosi 1
#define cs 0

//INITIALIZE SD CARD
void initSDCard();

//Write log data to the SD card, creating a new log file if needed
void writeLogData(uint16_t systemID, uint16_t baseStationID, String baseStationFirmwareVersion, uint8_t nodeID, uint16_t messageID, uint64_t data);

uint8_t readLogData(uint16_t systemID, uint16_t baseStationID, uint8_t nodeID, uint16_t messageID, uint16_t hourseToRead, JsonDocument *doc);

bool parseLogDataRow(char* line, uint64_t *time, uint32_t* nodeID, uint32_t* messageID, uint64_t* data);

//Get the log file name based on the systemID, baseStationID, and timeinfo
void getLogFilename(char* filename, uint16_t systemID, uint16_t baseStationID, tm timeinfo);

//Get the current log file name based on the current time
void getCurrentLogFilename(char* filename, uint16_t systemID, uint16_t baseStationID);

//Write the log file header
void writeLogHeader(File *file, uint16_t systemID, uint16_t baseStationID, String baseStationFirmwareVersion);

//Get string for a log data row based on the nodeID, messageID, and data
String getLogDataRow(uint8_t nodeID, uint16_t messageID, uint64_t data);

//Open the log file for writing, creating a new log file if needed
void openLogFile(File *file, uint16_t systemID, uint16_t baseStationID, String baseStationFirmwareVersion);

#endif