#include <SD.h>    //include SD card library
#include <SPI.h>

#define ECHO_TO_SERIAL  1 // echo data to serial port

//digital pins
#define Brakepin  2     //Brake Sensor Input from motorcycle
#define redLEDpin 5        //Red LED used for fatal error
#define chipSelect  10     //SD card

//analog pins
#define TPSpin  0             //Throttle Position Sensor Input from motorcycle

File logfile;    //the logging file

//Error function, used when fatal error (ie. can't read/write SD card).
//Turns on Red LED and sits in while(1) loop forever (aka halt).
void error(char *str){
  Serial.print("error: ");
  Serial.println(str);
  
  digitalWrite(redLEDpin, HIGH);
  
  while(1);
}

void setup(void){
  pinMode(redLEDpin, OUTPUT);     //Red debugging LED
  pinMode(chipSelect, OUTPUT);    //In order to write to SD Card, must be set as OUTPUT
  pinMode(Brakepin, INPUT);
  pinMode(TPSpin, INPUT);

  Serial.begin(38400);          //Initialize serial communications at 9600 bits per second    
  //see if card is present and can be initialized:
  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)){
    Serial.println("Card failed, or not present.");
    //don't do anthing more:
    return;
  }
  
  Serial.println("card initialized.");
  
  // create a new file
  char filename[] = "LOGGER00.CSV";
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = i/10 + '0';
    filename[7] = i%10 + '0';
    if (! SD.exists(filename)) {
      // only open a new file if it doesn't exist
      logfile = SD.open(filename, FILE_WRITE); 
      break;  // leave the loop!
    }
  }
  
  if (! logfile){
    error("could not create file");
  }
  
  Serial.print("Logging to: ");
  Serial.println(filename);
  
  //Text label heading:
  logfile.println("millis,tps,brake");
  #if ECHO_TO_SERIAL
  Serial.println("millis,tps,brake");  
  #endif //ECHO_TO_SERIAL
}

void loop(){
  //log milliseconds since starting
  uint32_t m = millis();
  logfile.print(m);      //milliseconds since start
  logfile.print(", ");
  #if ECHO_TO_SERIAL
  Serial.print(m);      //milliseconds since start
  Serial.print(", ");
  #endif //ECHO_TO_SERIAL  
  
  //read TPS value, convert to percentage, log
  int TPSValue = analogRead(TPSpin);
  TPSValue = map(TPSValue, 135, 793, 0, 100);
  //float TPSpercent = 100 * (TPSValue / 793.23); // TPS @ 100% is 3.877V
  logfile.print(TPSValue);
  logfile.print(", ");
  #if ECHO_TO_SERIAL
  Serial.print(TPSValue);
  Serial.print(", ");
  #endif //ECHO_TO_SERIAL

  //read Brake Value, log
  int BrakeValue = digitalRead(Brakepin);
  logfile.print(BrakeValue);
  logfile.print(", ");
  #if ECHO_TO_SERIAL
  Serial.print(BrakeValue);
  Serial.print(", ");
  #endif //ECHO_TO_SERIAL
  

  logfile.println();
  Serial.println();
  delay(1);
  logfile.flush();
}
