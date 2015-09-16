// Libraries
#include <TinyGPS.h>
#include <SPI.h>
#include <SD.h>

// Debug Condition
#if 1 // Need this statement or else rest of if else debug conditions won't work for some reason. 
__asm volatile("nop"); // Search Google for explanation.
#endif


#define DUEMILANOVE true // Set to true if using Duemilanove
#define DEBUG false  // Set to true if want to debug

#define DEBUG_Serial if(DEBUG)Serial // If DEBUG is set to true then all DEBUG_Serial statements will work
#if DUEMILANOVE 
  #if DEBUG //If debugging on Duemilanove set GPS to 3,2 and use SoftwareSerial
    #include <SoftwareSerial.h>
    #define RX_GPS_PIN 3
    #define TX_GPS_PIN 2
    SoftwareSerial GPS_Serial(3,2);
  #else //If not debugging on Duemilanove put GPS pins on 0,1
    #define RX_GPS_PIN 0
    #define TX_GPS_PIN 1
    #define GPS_Serial Serial
  #endif
#else // If using Mega 2560 set GPS pins to Serial1
  #define RX_GPS_PIN 19
  #define TX_GPS_PIN 20
  #define GPS_Serial Serial1
#endif

// Digital Pins
// Pins 0-3 reserved for GPS on Duemilanove depending on degugging condition
#define DEBUG_LED_PIN 4 // Debug on dash 
#define BRAKE_PIN 5 // +12v Input. Need Optocoupler to reduce to +5V input
#define SD_SWITCH_PIN 6 // Switch to close SD card
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

  // Intitialize input pins
  pinMode(BRAKE_PIN, INPUT);
  pinMode(TPS_PIN, INPUT);
  pinMode(SD_SWITCH_PIN, INPUT_PULLUP); // SD Switch set to normally HIGH, will go to LOW when pressed

  // Initialize output pins
  pinMode(DEBUG_LED_PIN, OUTPUT);
  pinMode(SD_PIN, OUTPUT);

  // Initialize HIGH pins
  digitalWrite(DEBUG_LED_PIN, HIGH);

  SERIAL_Initialize();
  SD_Check();
}

// There might be delay with GPS_GET by 1.5 seconds because data shows that MPH is increasing
// when TPS has already be let off. Need to test more and possibly find way to sync up
// Or just do during post processing and shift data rows accordingly
void loop() {
  if(digitalRead(SD_SWITCH_PIN) == LOW) { // If SD Button switch is pressed down
    dataFile.close(); // Then save and close file
    LED_FLASH(3); // And flash loop LED 3 times and halt
  }

  if(gps.encode(GPS_Serial.read())) { // If TinyGPS detects parsable data from serial port
    MOTO_Get(); // Then read input data
    GPS_Get(); // Get GPS Data
    SD_Write(); // Write to SD Card
  }
}

void SERIAL_Initialize() {
  // Initialize GPS and Debugging
  // iTeadStudio GPS default set at 38400 @ 1 Hz 
  // Go to uBlox uCenter > Edit > Messages > NMEA > PUBX to change Baud Rate
    // Go to uBlox uCenter > Edit > Messages > UBX > CFG > Rate to change Measurement Period.
    // Best results at 38400 bps @ 5 Hz on Arduino Hardware Serial.
    // If using SoftwareSerial MUST use 4800 bps @ 1 Hz.
  char baudRate[] = {0x24, 0x50, 0x55, 0x42, 0x58, 0x2C, 0x34, 0x31, 0x2C, 0x31, 0x2C, 0x30,
            0x30, 0x30, 0x37, 0x2C, 0x30, 0x30, 0x30, 0x33, 0x2C, 0x34, 0x38, 0x30,
            0x30, 0x2C, 0x30, 0x2A, 0x31, 0x33, 0x0D, 0x0A}; // Hex to change to 4800 bps
  char fiveHz[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xC8, 0x00, 0x01, 0x00, 0x01, 0x00, 0xDE, 0x6A}; //Hex to change to 5Hz
  char tenHz[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0x64, 0x00, 0x01, 0x00, 0x01, 0x00, 0x7A, 0x12}; // Hex to change to 10Hz
  
  #if DUEMILANOVE
    #if DEBUG
      Serial.begin(115200);
      GPS_Serial.begin(38400); 
      delay(1000);
      GPS_Serial.write(baudRate,sizeof(baudRate));
      GPS_Serial.begin(4800); // Set SoftwareSerial to 4800
    #else 
      GPS_Serial.begin(38400);
      delay(5000);
      GPS_Serial.write(tenHz,sizeof(tenHz));
      GPS_Serial.flush();
    #endif
  #else
    Serial.begin(115200);
    GPS_Serial.begin(38400);
    delay(5000);
    GPS_Serial.write(tenHz,sizeof(tenHz));
        GPS_Serial.flush();
  #endif  
}

void SD_Check() {
  DEBUG_Serial.print("Initializing SD card... ");
  if(!SD.begin(SD_PIN)) { // If SD SS Pin is false for whatever reason then
    DEBUG_Serial.println("Card failed, or not present.");
    LED_FLASH(1); // Flash loop LED once and halt
  }
  DEBUG_Serial.println("Card initialized.");

  // After SD Card has been initialized, file will be created once GPS has a lock
  // This is done by returning fileCreate boolean to true and will finally cause
  // program to leave while loop
  boolean fileCreate = false;
  while(fileCreate == false) {
    if(gps.encode(GPS_Serial.read())) {
      // Create a new file using character array. 
      // Must use '0' because if int i = 1, then i/10 = 0
      // and 0 + 0 = 'null' character
      // 0 + '0' = 0 + 48 = '0' character
      // Reference ASCII chart for dec to char value
      char filename[] = "LOGGER00.CSV"; // This creates array with 12 elements
      for (byte i = 0; i < 100; i++) {
        filename[6] = i/10 + '0'; // but 7th element is index 6
        filename[7] = i%10 + '0';
        if (!SD.exists(filename)) {
          // only open a new file if it doesn't exist
          dataFile = SD.open(filename, FILE_WRITE); 
          DEBUG_Serial.print("File ");
          DEBUG_Serial.print(filename);
          DEBUG_Serial.println(" has been created.");
          fileCreate = true;
          break;  // leave the loop! This is done so only one file is created
              // which is the first number that does not exist yet
        }
      }
    }
  }
  if (!dataFile){ // If for whatever reason file could not be created 
    DEBUG_Serial.println("could not create file.");
    LED_FLASH(2); // Flash loop led twice and halt
  }

  // CSV File header
  String header = "MM/DD/YYYY,HH:MM:SS.CC,Latitude,Longitude,MPH,TPS,Brake";
  dataFile.println(header);
  DEBUG_Serial.println(header);   
}

// LED sequence will blink how ever many times variable "flash" is set to, then pause for 2 seconds
// Then start blink sequence again indefinitely
void LED_FLASH(byte flash) {
  // Number of times LED flashes
  // flash = 1, SD Card failed or not present
  // flash = 2, Could not create file
  // flash = 3, SD card closed able to remove
  while(1)  { // while(1) causes program to be stuck in loop forever
    for (byte i = flash; i > 0; i--) {
      digitalWrite(DEBUG_LED_PIN, LOW);
      delay(200);
      digitalWrite(DEBUG_LED_PIN, HIGH);
      delay(200);
    }
    digitalWrite(DEBUG_LED_PIN, LOW);
    delay(2000);
    DEBUG_Serial.println("LED Light Blinking");
  }
}

void GPS_Get()  {

  digitalWrite(DEBUG_LED_PIN, LOW); // Once GPS has a lock then it will turn off LED pin

  // Read TinyGPS documentation on how to get necessary variables
  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths);
  gps.get_position(&lat, &lon);
  mph = byte(gps.f_speed_mph());

}

void MOTO_Get() {
  // 0% Throttle reads approx 0.66 V = 135, 100% Throttle reads 3.87 V = 793
  tpsValue = map(analogRead(TPS_PIN), 135, 793, 0 , 100); 

  brakeValue = map(digitalRead(BRAKE_PIN), 0, 1, 0, 100);
}

void SD_Write(){
  // Read up on sprintf for formatting array. 
  // ld = long variable, d = int, byte
  // 02 = must have two int on the left of decimal point
  // So if month is January which is equal to 1, then it will store in array as 01
  // This is done for formatting consistency 
  sprintf(dataArray, "%02d/%02d/%d,%02d:%02d:%02d.%02d,%ld,%ld,%d,%d,%d", 
          month, day, year, hour, minute, second, hundredths, 
          lat, lon, mph, tpsValue, brakeValue);
  dataFile.println(dataArray);
  DEBUG_Serial.println(dataArray);
  dataFile.flush(); // Save file, but doesn't close
}
