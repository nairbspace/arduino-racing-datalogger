#include <TinyGPS.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <SD.h>

// Flag to turn on/off serial debugging
// If DEBUG is set to false it will turn off all Serial statements
// If using Arduino with multiple Serial change accordingly (Serial1, Serial2, etc.)
// #include <Arduino.h> if necessary
#define DEBUG true 
#define Serial if(DEBUG)Serial 

TinyGPS gps;

//digital pins
//#define rxGPSPin 0 // Receiving data from GPS
//#define txGPSPin 1 // Transmitting data to GPS
#define brakePin 5 // 12v Brake Light input
SoftwareSerial softSerial(3, 2); // (rx on arduino , tx on arduino)
#define errorPin 4 // Light for when sd card fails to initialize
#define sdPin 10 // SD initialize pin
//#define mosiPin 11 // MOSI pin for sd card
//#define misoPin 12 // MISO pin for sd card
//#define sckPin 13 // SCK pin for sd card, takes up built in LED pin

//analog pins
#define tpsPin 0 // Throttle Position Sensor pin

String dataString = "";
String gpsString = "";
String motoString = "";
File dataFile;

// put your setup code here, to run once:
void setup() {

  //initialize input pins
  pinMode(brakePin, INPUT);
  pinMode(tpsPin, INPUT);

  //initialize output pins
  pinMode(errorPin, OUTPUT);
  pinMode(sdPin, OUTPUT);

  digitalWrite(errorPin, HIGH); // Will turn off in any sd card issues or could not create file

  Serial.begin(115200);

  SD_Check();
  
  //Header format for file to use for Text to Columns on Excel
  String header = "MM/DD/YYYY,HH:MM:SS.CC,Delay (ms),Latitude,Longitude,Altitude (ft),MPH,TPS,Brake";
  dataFile.println(header);
  Serial.println(header);

  // iTeadStudio GPS default set at 38400 @ 1 Hz. Go to uBlox uCenter > Edit > Messages > NMEA > PUBX
  // to get hex or string in order to change.
  // Also go to Edit > Messages > UBX > CFG > Rate to change Measurement Period.
  // Best results at 38400 bps @ 5 Hz on Arduino Hardware Serial.
  // If using SoftwareSerial must use 4800 bps @ 1 Hz.
  softSerial.begin(38400);
  softSerial.print("$PUBX,41,1,0007,0003,4800,0*13\r\n");
  //Serial.write("0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xC8, 0x00, 0x01, 0x00, 0x01, 0x00, 0xDE, 0x6A"); //change measurement period to 5 Hz
  softSerial.begin(4800);  

}

// put your main code here, to run repeatedly:
void loop() {  

  GPS_Parser();

  // If gpsString isn't blank then save to sdcard
  if (gpsString != "") {
    Moto_Parser();
    dataString += gpsString;
    dataString += ",";
    dataString += motoString;
    dataFile.println(dataString);
    Serial.println(dataString);
    
    // Reset strings after saving to sdcard
    gpsString = "";
    motoString = "";
    dataString = "";

    // Save file without closing
    dataFile.flush();

  }
}

// Read TinyGPS documentation and look at example sketches
void GPS_Parser() {
  float flat, flon, falt, fmph;
  unsigned long fix_age;
  int year;
  byte month, day, hour, minute, second, hundredths;

  while (softSerial.available())  {
    if (gps.encode(softSerial.read()))  {

      digitalWrite(errorPin, LOW); // turn off error pin

      gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths);
      gps.f_get_position(&flat, &flon, &fix_age);
      falt = gps.f_altitude() * 3.28084; //convert to feet
      fmph = gps.f_speed_mph();

      char dateTime[25];
      sprintf(dateTime, "%02d/%02d/%02d,%02d:%02d:%02d.%02d,", month, day, year, hour, minute, second, hundredths);
      gpsString += String(dateTime);

      gpsString += String(fix_age);

      gpsString += String(",");

      gpsString += String(flat, 6);
      gpsString += String(",");
      gpsString += String(flon, 6);
      gpsString += String(",");
      gpsString += String(falt, 2);
      gpsString += String(",");
      gpsString += String(fmph, 2);

    }
  } 
}

void Moto_Parser()  {
  // Must contain .0 to calculate as float
  float tpsPercent = 100 * (analogRead(tpsPin) / 1023.0); 

  byte brakePercent;
  if (digitalRead(brakePin) == HIGH) {
    brakePercent = 100;
  } else {
    brakePercent = 0;
  }

  motoString = String(tpsPercent, 0) + "," + String(brakePercent);
}

void SD_Check () {

  Serial.print("Initializing SD card... ");

  if(!SD.begin(sdPin))  {
    Serial.println("Card failed, or not present.");
    ERROR_Light(1000);
  }  

  Serial.println("card initialized.");

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
      break;  // leave the loop!
    }
  }

  if (!dataFile){
    Serial.println("could not create file.");
    ERROR_Light(100);
  }
}


// Program will end if goes into this function
// and loop LED light
void ERROR_Light(int ms)  {
  while(1) {
    digitalWrite(errorPin, LOW);
    delay(ms);
    digitalWrite(errorPin, HIGH);
    delay(ms);
    Serial.println("LED Light Blinking");
  }
}
