#ifndef dataLogging_h
#define dataLogging_h

#include <SPI.h>

#include <SD.h>

//SD Card Pins
#define sck 3
#define miso 2
#define mosi 1
#define cs 0

void initSDCard();

void writeLogData(uint16_t systemID, uint16_t baseStationID, String baseStationFirmwareVersion, uint8_t nodeID, uint16_t messageID, uint64_t data);

void getCurrentLogFilename(char* filename, uint16_t systemID, uint16_t baseStationID);

void writeLogHeader(File *file, uint16_t systemID, uint16_t baseStationID, String baseStationFirmwareVersion);

String getLogDataRow(uint8_t nodeID, uint16_t messageID, uint64_t data);

void openLogFile(File *file, uint16_t systemID, uint16_t baseStationID, String baseStationFirmwareVersion);

#endif