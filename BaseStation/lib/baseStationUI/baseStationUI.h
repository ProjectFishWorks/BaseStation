#ifndef baseStationUI_h
#define baseStationUI_h



#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
//#include "WiFiClientSecure.h"
//#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include <SPI.h>
#include <Wire.h>
#include <Adafruit_INA219.h>



//NeoPixel Setup Stuffs
#define PIN         45 // On Trinket or Gemma, suggest changing this to 1
#define NUMPIXELS   2 // Popular NeoPixel ring size
#define DELAYVAL    500 // Time (in milliseconds) to pause between pixels


//LCD Screen Setup Stuffs
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32


//I2C Pins
#define LCD_SDA 18
#define LCD_SCL 19
#define CUR_SDA 9
#define CUR_SCL 10

//UI Component declarations
// Adafruit_SSD1306 display;
// Adafruit_INA219 ina219;
// Adafruit_NeoPixel pixels;
// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// Adafruit_INA219 ina219(0x4A);  
// Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);
display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ina219 = Adafruit_INA219(0x4A);
pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);

//LCD UI Elements
#define NUMFLAKES     3 // Number of snowflakes in the animation example
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


//Function Prototypes

void initBaseStationUI();

void annoyingBuzz();

void drawBitmap(void);

void testCurrentSense ();

void configModeUI(String ssid);

void writeToLCD2lines(String line1, String line2);

void writeToPixel(int pixel, int r, int g, int b);

void sleepModeUI();


    

#endif