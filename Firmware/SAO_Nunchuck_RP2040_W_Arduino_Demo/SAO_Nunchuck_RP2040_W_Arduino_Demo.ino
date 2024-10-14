/* SAO Nunchuck Demo with Arduino 
  Github repo:  
  Project page: https://hackaday.io/project/198000-sao-nunchuck

  Dependencies. 
    This demo will work with a huge range of IDEs and hardware, but it was developed and tested with the following:
    Arduino IDE 2.3.2 
      Install "Arduino Mbed OS RP2040 Boards" v4.1.5 with Arduino Boards Manager
      The Wire Library is included.
      https://github.com/madhephaestus/WiiChuck/
    RP2040-Zero (miniature Pico/RP2040 dev board) https://www.waveshare.com/rp2040-zero.htm

  OLED 1.5" 128x128 SSD1327: https://learn.adafruit.com/adafruit-grayscale-1-5-128x128-oled-display/arduino-wiring-and-test

  Hardware Connections
    The SAO port bridges I2C and power (3.3V ONLY) to the Wii Nunchuck
  
*/

#include <Adafruit_SSD1327.h>
// #include <WiiChuck.h>
// Accessory nunchuck1;
#include <NintendoExtensionCtrl.h>
Nunchuk nchuk;

#define HARDWARE_VERSION_MAJOR  1
#define HARDWARE_VERSION_MINOR  0
#define HARDWARE_VERSION_PATCH  0
#define MCU_FAMILY              RP2040
  /***************************************** CORE4 HARDWARE VERSION TABLE *******************************************
  | VERSION |  DATE      | MCU     | DESCRIPTION                                                                    |
  -------------------------------------------------------------------------------------------------------------------
  |  1.0.0  | 2019-10-13 | RP2040  | First prototype, as built, getting it to come alive.
  |         |            |         |
  -----------------------------------------------------------------------------------------------------------------*/

#define OLED_ADDRESS                0x3C      // OLED SSD1327 is 0x3C
#define OLED_RESET                    -1
#define OLED_HEIGHT                  128
#define OLED_WIDTH                   128
#define OLED_WHITE                    15
#define OLED_GRAY                      7
#define OLED_BLACK                     0
Adafruit_SSD1327 display(OLED_HEIGHT, OLED_WIDTH, &Wire, OLED_RESET, 1000000);
uint8_t color = OLED_WHITE;
int16_t cursorX = 0;
int16_t cursorY = 0;

#define PIN_SAO_GPIO_1_ANA_POT_LEFT   A0      // Configured as left analog pot
#define PIN_SAO_GPIO_2_ANA_POT_RIGHT  A1      // Configured as left analog pot
int16_t PotLeftADCCounts = 0;
int16_t PotRightADCCounts = 0;
int16_t PotLeftADCCountsOld = 0;
int16_t PotRightADCCountsOld = 0;
int16_t PotMarginAtLimit = 10;
int16_t PotHystersisLimit = 12;
int16_t PotGlitchLimit = 64;
int16_t PotFilterSampleCount = 3;
int16_t deltaAbsolute = 0;

enum TopLevelMode                             // Top Level Mode State Machine
{
  MODE_INIT,
  MODE_BOOT_SCREEN,
  MODE_SKETCH,
  MODE_AT_THE_END_OF_TIME
} ;
uint8_t  TopLevelMode = MODE_INIT;
uint8_t  TopLevelModeDefault = MODE_BOOT_SCREEN;
uint32_t ModeTimeoutDeltams = 0;
uint32_t ModeTimeoutFirstTimeRun = true;

void setup()
{
  // Nothing to see here. Everything runs in a simple state machine in the main loop function, and the bottom of this file.
}

bool SerialInit() {
  bool StatusSerial = 1;
  Serial.begin(115200);
  // TODO get rid of this delay, quickly detect presence of serial port, and send message only if it's detected.
  delay(1500);
  Serial.println("\nHello World! This is the serial port talking!");
  return StatusSerial;
}

bool OLEDInit() {
  Serial.print(">>> INIT OLED started... ");
  if ( ! display.begin(0x3C) ) {
     Serial.println("Unable to initialize OLED");
     return 1;
  }
  else {
    Serial.println("Initialized!");
    OLEDClear();
  }
  return 0;
}

void NunchuckInit() {
  Serial.print(">>> INIT Nunchuck started...");
//  nunchuck1.type = NUNCHUCK;
//	nunchuck1.begin();
//	if (nunchuck1.type == Unknown) {
//		/** If the device isn't auto-detected, set the type explicatly
//		 * 	NUNCHUCK,
//		 WIICLASSIC,
//		 GuitarHeroController,
//		 GuitarHeroWorldTourDrums,
//		 DrumController,
//		 DrawsomeTablet,
//		 Turntable
//		 */
//		nunchuck1.type = NUNCHUCK;
//	}
	nchuk.begin();
  if (!nchuk.connect()) {
		Serial.println("Nunchuk not detected!");
	}
  else {
		Serial.println("Nunchuk connected.");
  }
}

void OLEDClear() {
  display.clearDisplay();
  display.display();
}

void OLEDFill() {
  display.fillRect(0,0,display.width(),display.height(),OLED_WHITE);
  display.display();
}

void ModeTimeOutCheckReset () {
  ModeTimeoutDeltams = 0;
  ModeTimeoutFirstTimeRun = true;
}

bool ModeTimeOutCheck (uint32_t ModeTimeoutLimitms) {
  static uint32_t NowTimems;
  static uint32_t StartTimems;
  NowTimems = millis();
  if(ModeTimeoutFirstTimeRun) { StartTimems = NowTimems; ModeTimeoutFirstTimeRun = false;}
  ModeTimeoutDeltams = NowTimems-StartTimems;
  if (ModeTimeoutDeltams >= ModeTimeoutLimitms) {
    ModeTimeOutCheckReset();
    Serial.print(">>> Mode timeout after ");
    Serial.print(ModeTimeoutLimitms);
    Serial.println(" ms.");
    return (true);
  }
  else {
    return (false);
  }
}

// -------------------------------------------------------------------------------------------
// MAIN LOOP STARTS HERE
// -------------------------------------------------------------------------------------------
void loop()
{
  switch(TopLevelMode) {
    case MODE_INIT: {
      SerialInit();
      Serial.println(">>> Entered MODE_INIT.");
      NunchuckInit();
      OLEDInit();
      Serial.println("");
      Serial.println("  |-------------------------------------------------------------------------------| ");
      Serial.println("  | Welcome to the SAO Nunchuck Demo with Arduino IDE 2.3.2 using RP2040-Zero!    | ");
      Serial.println("  |-------------------------------------------------------------------------------| ");
      Serial.println("");
      ModeTimeOutCheckReset(); 
      TopLevelMode = TopLevelModeDefault;
      Serial.println(">>> Leaving MODE_INIT.");
      break;
    }

    case MODE_BOOT_SCREEN: {
      static bool CursorState = 0;
      static uint32_t NowTime;
      static uint32_t LastTime;
      static uint32_t BlinkDeltaTime = 600;

      if (ModeTimeoutFirstTimeRun) { 
        Serial.println(">>> Entered MODE_BOOT_SCREEN."); 
        // Border
        uint8_t BorderThickness = 12;
        // display.fillRect(BorderThickness,BorderThickness,(display.width()-(2*BorderThickness)),(display.height()-(2*BorderThickness)),3);
        // display.display();
        // delay(1000);
        for (uint8_t i=0; i<BorderThickness; i+=1) {
          display.drawRect(i, i, display.width()-2*i, display.height()-2*i, OLED_WHITE);
        }
        display.display();
        delay(500);
        // Background
        // display.fillRect(2*BorderThickness,BorderThickness,2*BorderThickness,2*BorderThickness,OLED_GRAY);
        //for (uint8_t i=BorderThickness; i<display.height(); i+=1) {
        //  display.drawRect(i, i, display.width()-2*i-BorderThickness, display.height()-2*i-BorderThickness, OLED_GRAY);
        //}
        // display.setFont(&FreeMono9pt7b);
        display.setTextSize(1);
        // display.cp437(true);
        display.setTextColor(SSD1327_WHITE);
        display.setCursor(20,20);
        display.println("128 Bytes Free");
        display.setCursor(12,40);
        display.println("Ready.");
        display.display();
      }

      // Is it time to blink?
      NowTime = millis();
      if ( (NowTime-LastTime) > BlinkDeltaTime) {
        // Blink the cursor
        LastTime = NowTime;
        if (CursorState) { display.fillRect(12,47,8,10,SSD1327_WHITE); CursorState = 0;}
        else { display.fillRect(12,47,8,10,SSD1327_BLACK);  CursorState = 1;}
        display.display();
      }

      if (ModeTimeOutCheck(20000)){ 
        ModeTimeOutCheckReset();
        OLEDClear();
        TopLevelMode++; 
        Serial.println(">>> Leaving MODE_BOOT_SCREEN.");
      }
      break;
    }

    case MODE_SKETCH: {
      if (ModeTimeoutFirstTimeRun) { 
        Serial.println(">>> Entered MODE_SKETCH.");
        OLEDFill();
        ModeTimeoutFirstTimeRun = false;
      }
      // Arduino Analog Reading
      PotLeftADCCounts = analogRead(PIN_SAO_GPIO_1_ANA_POT_LEFT);
      PotRightADCCounts = analogRead(PIN_SAO_GPIO_2_ANA_POT_RIGHT);

      // nunchuck1.readData();    // Read inputs and update maps
      // nunchuck1.printInputs(); // Print all inputs

      boolean success = nchuk.update();  // Get new data from the controller

      if (!success) {  // Ruh roh
        Serial.println("Controller disconnected!");
        delay(1000);
      }
      else {
        // Read a button (on/off, C and Z)
        boolean zButton = nchuk.buttonZ();

        Serial.print("The Z button is ");
        if (zButton == true) {
          Serial.println("pressed");
        }
        else if (zButton == false) {
          Serial.println("released");
        }

        // Read a joystick axis (0-255, X and Y)
        int joyY = nchuk.joyY();

        Serial.print("The joystick's Y axis is at ");
        Serial.println(joyY);

        // Read an accelerometer and print values (0-1023, X, Y, and Z)
        int accelX = nchuk.accelX();

        Serial.print("The accelerometer's X-axis is at ");
        Serial.println(accelX);

        // Print all the values!
        nchuk.printDebug();
      }


      // Apply glitch rejection

      // Apply some filtering

      // Apply some hysteresis
      deltaAbsolute = abs(PotLeftADCCounts-PotLeftADCCountsOld);
      if (deltaAbsolute < PotHystersisLimit) {
        PotLeftADCCounts = PotLeftADCCountsOld;
      }
      deltaAbsolute = abs(PotRightADCCounts-PotRightADCCountsOld);
      if (deltaAbsolute < PotHystersisLimit) {
        PotRightADCCounts = PotRightADCCountsOld;
      }
      // Scale ADC counts to the useable screen with margins at end of pot travel.
      cursorX = map(PotLeftADCCounts , (1023-PotMarginAtLimit), PotMarginAtLimit, 0, 127);
      cursorY = map(PotRightADCCounts, PotMarginAtLimit, (1023-PotMarginAtLimit), 0, 127);
      PotLeftADCCountsOld = PotLeftADCCounts;
      PotRightADCCountsOld = PotRightADCCounts;
      display.drawRect(cursorX, cursorY, 2, 2, OLED_BLACK);
      display.display();
      break;
    }

    case MODE_AT_THE_END_OF_TIME: {
      Serial.println(">>> Stuck in the MODE_AT_THE_END_OF_TIME! <<<");
      break;
    }

    default: {
      Serial.println(">>> Something is broken! <<<");
      break;
    }
  }
}
