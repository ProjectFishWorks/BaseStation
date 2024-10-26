#include "baseStationUI.h"





//Initialize the UI components (SETUP FUNCTION)
void initBaseStationUI() {

    pinMode(0, INPUT_PULLUP);  //Program button with pullup
    pinMode(21, INPUT_PULLUP); //ButtBlue
    pinMode(47, INPUT_PULLUP); //ButtWhite
    pinMode(48, OUTPUT); //Buzzer
    pinMode(11, OUTPUT); //CAN Bus Power
    digitalWrite(11, LOW);

    
    Wire.begin(LCD_SDA, LCD_SCL);
    Wire1.begin(CUR_SDA, CUR_SCL);


    //UI Component declarations
    // Adafruit_SSD1306 display;
    // Adafruit_INA219 ina219;
    // Adafruit_NeoPixel pixels;
    // Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
    // Adafruit_INA219 ina219(0x4A);  
    // Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);
    // display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
    // ina219 = Adafruit_INA219(0x4A);
    // pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);
        

    
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      Serial.println(F("SSD1306 allocation failed"));
      for(;;); // Don't proceed, loop forever
      }
    if (! ina219.begin(&Wire1)) {
      Serial.println("Failed to find INA219 chip");
      while (1) { delay(10); }
    }
    ina219.setCalibration_16V_400mA();

    pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
    

    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.
    display.clearDisplay();
    display.drawBitmap(
      (display.width()  - LOGO_WIDTH ) / 2,
      (display.height() - LOGO_HEIGHT) / 2,
      logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);  
    display.display();
    Serial.println(F("Initialized LCD Screen"));



    //NeoPixel Setup Stuffs part 2
    pixels.clear(); // Set all pixel colors to 'off'
    for (int i=0; i<NUMPIXELS; i++){
      pixels.setPixelColor(i, pixels.Color(150, 150, 0));
      //pixels.setPixelColor(1, pixels.Color(0, 150, 0));
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
    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.println(F("Loading..."));
    display.setCursor(20, 30);
    display.println(F("Please Wait"));
    display.display(); // Show initial text
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
void drawBitmap(void) {

    display.clearDisplay();
    display.drawBitmap(
        (display.width()  - LOGO_WIDTH ) / 2,
        (display.height() - LOGO_HEIGHT) / 2,
        logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
    display.display();
}

//Test stuff for Current Sense
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
    display.println(F("Current is:"));
    display.setTextSize(1); // Draw 1X-scale text
    display.setCursor(1, 30);
    display.print(current_mA);
    display.print(" mA");
    display.display();      // Show initial text
    delay(2000);

    //testanimate(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT);    // Draw a small bitmap image
    Serial.println("Success");
}


//UI for when the base station is in config mode
void configModeUI(String ssid) {
    pixels.setPixelColor(1, pixels.Color(0, 0, 150)); //Turn status pixel to blue
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
    display.print(ssid);
    display.println(F(" >"));
    display.setCursor(5, 32);
    display.println(F("to enter your WiFi"));
    display.setCursor(5, 41);
    display.println(("at the web page: "));
    display.setCursor(5, 50);
    display.print(F("http://"));
    display.println(WiFi.softAPIP());
    display.display();      // Show initial text
}

//Write to the LCD screen
void writeToLCD2lines(String line1, String line2) {
    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(1, 10);
    display.println(line1);
    display.setCursor(1, 30);
    display.println(line2);
    display.display();      // Show initial text
}

//Write to the NeoPixel
void writeToPixel(int pixel, int r, int g, int b) {
    pixels.setPixelColor(pixel, pixels.Color(r, g, b));
    pixels.show(); // Set all pixels
}

 void sleepModeUI() {
    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.println(F("Entering Standby"));
    display.setCursor(20, 30);
    display.println(F("Good Night~~"));
    display.display(); // Show initial text
    delay(1000);
    drawBitmap();
    pixels.setPixelColor(0, pixels.Color(40, 0, 0)); //Dim pwr pixel
    pixels.setPixelColor(1, pixels.Color(0, 0, 0)); //Turn off status pixel
    pixels.show(); // Set all pixels
}
