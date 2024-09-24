#ifndef dataLogging_h
#define dataLogging_h

#include <SPI.h>

#include <SD.h>

//SD Card Pins
#define sck 3
#define miso 2
#define mosi 1
#define cs 0

//INITIALIZE SD CARD
void initSDCard();

//Write log data to the SD card, creating a new log file if needed
void writeLogData(uint16_t systemID, uint16_t baseStationID, String baseStationFirmwareVersion, uint8_t nodeID, uint16_t messageID, uint64_t data);

//Get the current log file name based on the current time
void getCurrentLogFilename(char* filename, uint16_t systemID, uint16_t baseStationID);

//Write the log file header
void writeLogHeader(File *file, uint16_t systemID, uint16_t baseStationID, String baseStationFirmwareVersion);

//Get string for a log data row based on the nodeID, messageID, and data
String getLogDataRow(uint8_t nodeID, uint16_t messageID, uint64_t data);

//Open the log file for writing, creating a new log file if needed
void openLogFile(File *file, uint16_t systemID, uint16_t baseStationID, String baseStationFirmwareVersion);

#endif