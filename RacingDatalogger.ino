// Libraries
#include <TinyGPS.h>
#include <SPI.h>
#include <SD.h>

// Can comment out this statement if it compiles. Bug on certain Arduino IDE versions.
// Read here for more info: http://subethasoftware.com/2013/04/09/arduino-compiler-problem-with-ifdefs-solved/
#if 1
__asm volatile("nop"); 
#endif

// Debug Condition
#define DUEMILANOVE true // Set to true if using Duemilanove
#define DEBUG false  // Set to true if want to debug

#define SERIAL_DEBUG if(DEBUG)Serial // If DEBUG is set to true then all SERIAL_DEBUG statements will work
#if DUEMILANOVE 
  #if DEBUG //If debugging on Duemilanove set GPS to 3,2 and use SoftwareSerial
    #include <SoftwareSerial.h>
    #define RX_GPS_PIN 3
    #define TX_GPS_PIN 2
    SoftwareSerial SERIAL_GPS(3,2);
  #else //If not debugging on Duemilanove put GPS pins on 0,1
    #define RX_GPS_PIN 0
    #define TX_GPS_PIN 1
    #define SERIAL_GPS Serial
  #endif
#else // If using Mega 2560 set GPS pins to Serial1
  #define RX_GPS_PIN 19
  #define TX_GPS_PIN 20
  #define SERIAL_GPS Serial1
#endif

// Digital Pins
// Pins 0-3 reserved for GPS on Duemilanove depending on degugging condition
#define LED_STATUS_PIN 4 // LED pin to show current status, helpful for debugging.
#define BRAKE_PIN 5 // +12v Input. Need Optocoupler to reduce to +5V input
#define STOP_SWITCH_PIN 6 // Pushbutton switch to stop program
#define SD_PIN 10 // SD SS Pin
#define MOSI_PIN 11 // SD MOSI Pin
#define MISO_PIN 12 // SD MISO Pin
#define SCK_PIN 13 // SD SCK Pin

// Analog Pins
#define TPS_PIN A0 // Throttle Position Sensor

// Initialize TinyGPS Variables
TinyGPS gps;
int year; 
long lat, lon;
byte month, day, hour, minute, second, hundredths, mph;

// Initialize SD File Variable
File dataFile;

// Initialize Global Variables
int brakeValue, tpsValue;
char dataArray[60]; // Array for writing line of data receieved from GPS, Brake, TPS, etc. to SD Card

void setup() {
  setIoPins(); // Initialize IO Pins
  setSerial(); // Initialize Serial ports
  setSdCard(); // Initialize SD Card
  createFile(); // Create File
}

// TODO: Need to test timing accuracy with GPS vs Sensor Data.
// GPS Data is currently showing MPH increasing while TPS has already been let off 1.5 seconds ago.
// Might be best to do post processing on data and shift accordingly.
void loop() {
  checkStopSwitchPin();

  if(gps.encode(SERIAL_GPS.read())) { // If TinyGPS detects parsable data from serial port
    getSensorData(); // Then get sensor data
    setGpsData(); // Get GPS data
    writeToSdCard(); // Write data to SD Card
  }
}

void setIoPins() {
  // Intitialize input pins
  pinMode(BRAKE_PIN, INPUT);
  pinMode(TPS_PIN, INPUT);
  pinMode(STOP_SWITCH_PIN, INPUT_PULLUP); // SD Switch set to normally HIGH, will go to LOW when pressed

  // Initialize output pins
  pinMode(LED_STATUS_PIN, OUTPUT);
  pinMode(SD_PIN, OUTPUT);

  // Initialize HIGH pins
  digitalWrite(LED_STATUS_PIN, HIGH);
}

// Initialize GPS and Debugging
// iTeadStudio GPS Shield set at 38400 bps @ 1 Hz by default (on my unit)
// GPS Shield uses NEO-6M GPS Module which can be configured by downloading uBlox uCenter on computer.
// Go to uBlox uCenter > Edit > Messages > NMEA > PUBX to change Baud Rate and get Hex.
// Go to uBlox uCenter > Edit > Messages > UBX > CFG > Rate to change Measurement Period and get Hex.
// Best results at 38400 bps @ 5 Hz on Arduino Hardware Serial.
// If using SoftwareSerial MUST set GPS Module to 4800 bps @ 1 Hz.
void setSerial() {
  char baudRate[] = {0x24, 0x50, 0x55, 0x42, 0x58, 0x2C, 0x34, 0x31, 0x2C, 0x31, 0x2C, 0x30,
            0x30, 0x30, 0x37, 0x2C, 0x30, 0x30, 0x30, 0x33, 0x2C, 0x34, 0x38, 0x30,
            0x30, 0x2C, 0x30, 0x2A, 0x31, 0x33, 0x0D, 0x0A}; // Hex to change to 4800 bps
  char fiveHz[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xC8, 0x00, 0x01, 0x00, 0x01, 0x00, 0xDE, 0x6A}; //Hex to change to 5Hz
  char tenHz[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0x64, 0x00, 0x01, 0x00, 0x01, 0x00, 0x7A, 0x12}; // Hex to change to 10Hz
  
  #if DUEMILANOVE
    #if DEBUG
      SERIAL_DEBUG.begin(115200);
      SERIAL_GPS.begin(38400); 
      delay(1000);
      SERIAL_GPS.write(baudRate,sizeof(baudRate));
      SERIAL_GPS.begin(4800); // Set SoftwareSerial to 4800
    #else 
      SERIAL_GPS.begin(38400);
      delay(5000);
      SERIAL_GPS.write(tenHz,sizeof(tenHz));
      SERIAL_GPS.flush();
    #endif
  #else
    SERIAL_DEBUG.begin(115200);
    SERIAL_GPS.begin(38400);
    delay(5000);
    SERIAL_GPS.write(tenHz,sizeof(tenHz));
        SERIAL_GPS.flush();
  #endif  
}

void setSdCard() {
  SERIAL_DEBUG.print("Initializing SD card... ");
  if(!SD.begin(SD_PIN)) { // If SD SS Pin is false for whatever reason then
    SERIAL_DEBUG.println("Card failed, or not present.");
    flashLed(1); // Flash loop LED once and halt
  }
  SERIAL_DEBUG.println("Card initialized.");    
}

// After SD Card has been initialized, file will be created once GPS has a lock.
// This is done by setting isFileCreated boolean to true and will finally cause
// program to leave while loop.
// TODO: Reduce control flow inception (ie. too many loops within loops) for sanity purposes.
void createFile() { 
  boolean isFileCreated = false;
  while(!isFileCreated) {
    if(gps.encode(SERIAL_GPS.read())) {
      // Create a new file using character array. 
      // Must use '0' because if int i = 1 then i/10 = 0.1 
      // but since its an integer, i/10 = 0 due to rounding cutoff
      // and 0 + 0 = 'null' character
      // but 0 + '0' = 0 + 48 = '0' character
      // Reference ASCII chart for dec to char value
      char filename[] = "LOGGER00.CSV"; // This creates array with 12 elements
      for (byte i = 0; i < 100; i++) {
        filename[6] = i/10 + '0'; // but 7th element is index 6
        filename[7] = i%10 + '0';
        if (!SD.exists(filename)) {
          // only create a new file if it doesn't exist
          dataFile = SD.open(filename, FILE_WRITE); 
          SERIAL_DEBUG.print("File ");
          SERIAL_DEBUG.print(filename);
          SERIAL_DEBUG.println(" has been created.");
          isFileCreated = true;
          break;  // leave the for loop! This is done so only one file is created
              // which is the first number that does not exist yet
        }
      }
    }
  }

  if (!dataFile){ // If for whatever reason file could not be created 
    SERIAL_DEBUG.println("could not create file.");
    flashLed(2); // Flash loop led twice and halt
  }

  // CSV File header
  String header = "MM/DD/YYYY,HH:MM:SS.CC,Latitude,Longitude,MPH,TPS,Brake";
  dataFile.println(header);
  SERIAL_DEBUG.println(header);
}

void checkStopSwitchPin() {
  if(digitalRead(STOP_SWITCH_PIN) == LOW) { // If Stop pushbutton switch is pressed down then
    dataFile.close(); // Save and close file
    flashLed(3); // And flash loop LED 3 times and halt
  }
}

// LED sequence will blink how ever many times flash variable is set to, 
// pause for 2 seconds, then start blink sequence over again indefinitely.
// Number of times LED flashes:
  // 1 = SD Card failed or not present
  // 2 = Could not create file
  // 3 = SD card closed able to remove
void flashLed(byte flash) {
  while(1)  { // while(1) causes program to be stuck in loop forever
    for (byte i = flash; i > 0; i--) {
      digitalWrite(LED_STATUS_PIN, LOW);
      delay(200);
      digitalWrite(LED_STATUS_PIN, HIGH);
      delay(200);
    }
    digitalWrite(LED_STATUS_PIN, LOW);
    delay(2000);
    SERIAL_DEBUG.println("LED Light Blinking");
  }
}

void setGpsData() {
  digitalWrite(LED_STATUS_PIN, LOW); // Once GPS has a lock then it will turn off LED pin

  // Read TinyGPS documentation on how to get necessary variables
  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths);
  gps.get_position(&lat, &lon);
  mph = byte(gps.f_speed_mph());

}

void getSensorData() {
  // 0% Throttle reads approx 0.66 V = 135, 100% Throttle reads 3.87 V = 793
  tpsValue = map(analogRead(TPS_PIN), 135, 793, 0 , 100); 
  brakeValue = map(digitalRead(BRAKE_PIN), 0, 1, 0, 100);
}

// Read up on sprintf for formatting array. 
// ld = long variable, d = int, byte
// 02 = must have two int on the left of decimal point
// So if month is January which is equal to 1, then it will store in array as 01
// This is done for formatting consistency.
void writeToSdCard(){ 
  sprintf(dataArray, "%02d/%02d/%d,%02d:%02d:%02d.%02d,%ld,%ld,%d,%d,%d", 
          month, day, year, hour, minute, second, hundredths, 
          lat, lon, mph, tpsValue, brakeValue);
  dataFile.println(dataArray);
  SERIAL_DEBUG.println(dataArray);
  dataFile.flush(); // Save file, but doesn't close
}
