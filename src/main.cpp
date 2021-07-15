#include <Arduino.h>
#include <RTClib.h>
#include <SD.h>
#include <Ethernet.h>
#include <Wire.h>
#include <SPI.h>
#include<Arduino_FreeRTOS.h>
#include <LiquidCrystal_I2C.h>

#define SDCARD_CS 4
#define WIZ_CS 10
#define BUFSIZ 100
File root;

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip[] = {192,168,0,177};
EthernetServer server(80);
RTC_DS3231 rtc;
// store error strings in flash to save RAM
#define error(s) error_P(PSTR(s))

int sensor1, sensor2, sensor3, sensor1_mm, sensor2_mm, sensor3_mm;
int valve_1_state = LOW;
int valve_2_state = LOW;
int valve_3_state = LOW;
const int valve_1 = 22;
const int valve_2 = 25;
const int valve_3 = 27;
const int pingPin = 7; // Trigger Pin of Ultrasonic Sensor
const int echoPin = 6; // Echo Pin of Ultrasonic Sensor
float cm;
LiquidCrystal_I2C lcd(0x27,16,2); 


void error_P(const char* str) {
  Serial.print(F("error: "));
  Serial.println(str);

  while(1);

}

float microsecondsToCentimeters(long microseconds) {
   return microseconds / 2 * 0.343;
}
void measure() {
   long duration;
   
   pinMode(pingPin, OUTPUT);
   digitalWrite(pingPin, LOW);
   delayMicroseconds(5);
   digitalWrite(pingPin, HIGH);
   delayMicroseconds(10);
   digitalWrite(pingPin, LOW);
   pinMode(echoPin, INPUT);
   duration = pulseIn(echoPin, HIGH);
   cm = microsecondsToCentimeters(duration);
  // Serial.print(cm);
   //Serial.print("mm");
  // Serial.println();
  // delay(500);
   lcd.setCursor(0,0);
   lcd.print("Depth");
   lcd.setCursor(0,1);
   lcd.print(cm);
   lcd.print(" mm");
   lcd.print(" ");
}

void printDirectory(File dir, int numTabs) {
   while(true) {
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');
     }
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numTabs+1);
     } else {
       // files have sizes, directories do not
       Serial.print("\t\t");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}



void time(){
 DateTime now = rtc.now();

    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" (");
    //Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
    
}
void sensors(){
  sensor1 = analogRead(A0);
  sensor1_mm = map(sensor1,519,750,25,250);
  sensor2 = analogRead(A1);
  sensor2_mm = map(sensor2,0,1023,0,300);
  sensor3 = analogRead(A2);
  sensor3_mm = map(sensor3,0,1023,0,300);
}



void setup() {
  // put your setup code here, to run once:
 

  pinMode(valve_1, OUTPUT);
  pinMode(valve_2, OUTPUT);
  pinMode(valve_3, OUTPUT);
  Serial.begin(115200);
  if (!SD.begin(SDCARD_CS)) {
    error("card.init failed!");
  } 
  
  root = SD.open("/");
  printDirectory(root, 0);
  
  // Recursive list of all directories
  Serial.println(F("Files found in all dirs:"));
  printDirectory(root, 0);
  
  Serial.println();
  Serial.println(F("Done"));
  
  // Debugging complete, we start the server!
  Serial.println(F("Initializing WizNet"));
  Ethernet.init(WIZ_CS);
  delay(1000);
  Ethernet.begin(mac, ip);
  Serial.print(F("My IP address: "));
  Serial.println(Ethernet.localIP());

  server.begin();
lcd.init();                      // initialize the lcd 
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Hello, world!");
  lcd.setCursor(0,1);
  lcd.print("Paul Doughty");

 if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
     if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
 }
  
   lcd.clear();
}

void ListFiles(EthernetClient client, uint8_t flags, File dir) {
  client.println("<ul>");
  while (true) {
    File entry = dir.openNextFile();
   
    // done if past last used entry
     if (! entry) {
       // no more files
       break;
     }

    // print any indent spaces
    client.print("<li><a href=\"");
    client.print(entry.name());
    if (entry.isDirectory()) {
       client.println("/");
    }
    client.print("\">");
    
    // print file name with possible blank fill
    client.print(entry.name());
    if (entry.isDirectory()) {
       client.println("/");
    }
        
    client.print("</a>");
/*
    // print modify date/time if requested
    if (flags & LS_DATE) {
       dir.printFatDate(p.lastWriteDate);
       client.print(' ');
       dir.printFatTime(p.lastWriteTime);
    }
    // print size if requested
    if (!DIR_IS_SUBDIR(&p) && (flags & LS_SIZE)) {
      client.print(' ');
      client.print(p.fileSize);
    }
    */
    client.println("</li>");
    entry.close();
  }
  client.println("</ul>");
}

void web() {
  char clientline[BUFSIZ];
  char name[17];
  int index = 0;
  EthernetClient client = server.available();
  if (client) {
    // an http request ends with a blank line
    boolean current_line_is_blank = true;
    
    // reset the input buffer
    index = 0;
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        
        // If it isn't a new line, add the character to the buffer
        if (c != '\n' && c != '\r') {
          clientline[index] = c;
          index++;
          // are we too big for the buffer? start tossing out data
          if (index >= BUFSIZ) 
            index = BUFSIZ -1;
          
          // continue to read more data!
          continue;
        }
        
        // got a \n or \r new line, which means the string is done
        clientline[index] = 0;
        
        // Print it out for debugging
        Serial.println(clientline);
        
        // Look for substring such as a request to get the file
        if (strstr(clientline, "GET /") != 0) {
          // this time no space after the /, so a sub-file!
          char *filename;
          
          filename = clientline + 5; // look after the "GET /" (5 chars)  *******
          // a little trick, look for the " HTTP/1.1" string and 
          // turn the first character of the substring into a 0 to clear it out.
          (strstr(clientline, " HTTP"))[0] = 0;
 
          if(filename[strlen(filename)-1] == '/') {  // Trim a directory filename
            filename[strlen(filename)-1] = 0;        //  as Open throws error with trailing /
          }
          
          Serial.print(F("Web request for: ")); Serial.println(filename);  // print the file we want

          File file = SD.open(filename, O_READ);
          if ( file == 0 ) {  // Opening the file with return code of 0 is an error in SDFile.open
            client.println("HTTP/1.1 404 Not Found");
            client.println("Content-Type: text/html");
            client.println();
            client.println("<h2>File Not Found!</h2>");
            client.println("<br><h3>Couldn't open the File!</h3>");
            break; 
          }
          
          Serial.println("File Opened!");
                    
          client.println("HTTP/1.1 200 OK");
          if (file.isDirectory()) {
            Serial.println("is a directory");
            //file.close();
            client.println("Content-Type: text/html");
            client.println();
            client.print("<h2>Files in /");
            client.print(filename); 
            client.println(":</h2>");
            ListFiles(client,LS_SIZE,file);  
            file.close();                   
          } else { // Any non-directory clicked, server will send file to client for download
            client.println("Content-Type: application/octet-stream");
            client.println();
          
            char file_buffer[16];
            int avail;
            while (avail = file.available()) {
              int to_read = min(avail, 16);
              if (to_read != file.read(file_buffer, to_read)) {
                break;
              }
              // uncomment the serial to debug (slow!)
              //Serial.write((char)c);
              client.write(file_buffer, to_read);
            }
            file.close();
          }
        } else {
          // everything else is a 404
          client.println("HTTP/1.1 404 Not Found");
          client.println("Content-Type: text/html");
          client.println();
          client.println("<h2>File Not Found!</h2>");
        }
        break;
      }
    }
    // give the web browser time to receive the data
    delay(1);
    client.stop();
  }
}

void loop() {
 web();
 measure();
 if (cm < 300){
  digitalWrite (valve_1, HIGH);
 }
 if (cm > 302){
   digitalWrite( valve_1, LOW);
 }
}