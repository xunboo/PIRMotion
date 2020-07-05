#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <esp8266-google-home-notifier.h>
#include <FS.h> 
#include <NTPClient.h>
#include "DataToMaker.h"

const char* ssid     = "";
const char* password = "";
const char* ifttt_key = "";
const char* filename = "/link.txt";

const int outPin = 14; //D5 is 14 from d1_mini/pins_arduino.h; // LED connected to digital pin 5
const int inPin = 5; //D1 is 5 From d1_mini/pins_arduino.h;   // PIR Motion connected to digital pin 1
int val = 0;     // variable to store the read value
int pirState = LOW;             // we start, assuming no motion detected
int wifiState = WL_IDLE_STATUS;
bool googleConnected = false;   
int connectCount = 0;
unsigned long timestamp = 0;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -8*3600;    //PST:GMT-8
const int   daylightOffset_sec = 3600;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

ESP8266WebServer server(80);
GoogleHomeNotifier ghn;

// declare new maker event with the name "ESP"
DataToMaker event(ifttt_key, "Motion");


/////////////////////////////////////////////////////////////////////////

void setup() {
  // put your setup code here, to run once:
  Serial.begin(74880);
  Serial.println("");
  Serial.print("connecting to Wi-Fi");

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(outPin, OUTPUT);
  pinMode(inPin, INPUT);

  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(outPin, LOW);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  wifiState = WiFi.status();

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  wifiState = WiFi.status();
  
  Serial.println("");
  Serial.println("connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //Print the local IP

  digitalWrite(LED_BUILTIN, LOW);

  //init and get the time
  timeClient.begin();
  
  Serial.println(timeClient.getFormattedTime());
  timestamp = timeClient.getEpochTime();

  //Initialize File System
  if(SPIFFS.begin())
  {
    Serial.println("SPIFFS Initialize....ok");
  }
  else
  {
    Serial.println("SPIFFS Initialization...failed");
  }
  handleFileSystem();

  connectGoogleDevice();
}

void connectGoogleDevice(){
  const char displayName[] = "Living Room Speaker";

  Serial.println("connecting to Google Home...");
  if (ghn.device(displayName, "en") != true) {
    Serial.println(ghn.getLastError());
    return;
  }
  
  googleConnected = true;
  Serial.print("found Google Home(");
  Serial.print(ghn.getIPAddress());
  Serial.print(":");
  Serial.print(ghn.getPort());
  Serial.println(")");

  ghn.notify("ESP Motion detector is online");
  
  server.on("/speech", handleSpeechPath);
  server.on("/", handleRootPath);
  server.begin();
}

void handleFileSystem(){
  //Format File System
  /*if(SPIFFS.format())
  {
    Serial.println("File System Formated");
  }
  else
  {
    Serial.println("File System Formatting Error");
  }*/
 
  //Create New File And Write Data to It
  //w=Write Open file for writing
  File fread = SPIFFS.open(filename, "r");
  
  if (!fread) {
    Serial.println("file open failed");

    File fwrite = SPIFFS.open(filename, "w");
    if(!fwrite)
    {
      //Create/Write data to file
      Serial.println("Writing Data to File");
      fwrite.print("null");
      fwrite.close();
    }
  }
  else
  {
    Serial.println("read file...");
    
    if(fread.size()> 0){
      for(int i=0;i<fread.size();i++) //Read upto complete file size
      {
        Serial.print((char)fread.read());
      }
    }

    fread.close();  //Close file
    Serial.println();

    File fwrite = SPIFFS.open(filename, "w");
    fwrite.print("abc");
    fwrite.close();
  }
}

void handleSpeechPath() {
  String phrase = server.arg("phrase");
  if (phrase == "") {
    server.send(401, "text/plain", "query 'phrase' is not found");
    return;
  }
  
  if (phrase.startsWith("http") && phrase.endsWith(".mp3")) {
    Serial.print("play ");
    Serial.println(phrase.c_str());
    if (ghn.play(phrase.c_str()) != true) {
      Serial.println(ghn.getLastError());
      server.send(500, "text/plain", ghn.getLastError());     
    }
    return;
  }
  
  if (ghn.notify(phrase.c_str()) != true) {
    Serial.println(ghn.getLastError());
    server.send(500, "text/plain", ghn.getLastError());
    return;
  }
  server.send(200, "text/plain", "OK");
}

void handleRootPath() {
  server.send(200, "text/html", "<html><head></head><body><input type=\"text\"><button>speech</button><script>var d = document;d.querySelector('button').addEventListener('click',function(){xhr = new XMLHttpRequest();xhr.open('GET','/speech?phrase='+encodeURIComponent(d.querySelector('input').value));xhr.send();});</script></body></html>");
}

void handleMotionDetect() {
  val = digitalRead(inPin);  // read input value
  
  if (val == HIGH)  // check if the input is HIGH, Motion dectected
  {            
    digitalWrite(outPin, HIGH);  // turn OUT ON
  
    if (pirState == LOW) 
    {    
      Serial.print(timeClient.getFormattedTime());
      Serial.println("-- Motion detected!"); // print on output change
      pirState = HIGH;

      if (event.connect())    // send IFTTT webhook
      {
        event.post();
      }
    }

    timestamp = timeClient.getEpochTime();
  } 
  else if((timeClient.getEpochTime() - timestamp) > 60)  // keep OUT on 60s
  {
    digitalWrite(outPin, LOW); // turn OUT OFF
  
    if (pirState == HIGH)
    {
      Serial.print(timeClient.getFormattedTime());
      Serial.println("-- Motion ended!");  // print on output change
      pirState = LOW;
    }
  }
  /*else
  {
    Serial.println("..."); 
    Serial.print(timeClient.getFormattedTime());
    Serial.println("..."); 
  }*/
}

void loop() {
  // put your main code here, to run repeatedly:
  if(wifiState == WL_CONNECTED){
    timeClient.update();
  
    if(googleConnected){
      server.handleClient();
    }
    else if(((connectCount++) % 100) == 0){
      connectGoogleDevice();
    }

    handleMotionDetect();
  }
  else{
    Serial.print(".");
    googleConnected = false;
    connectCount= 0;
  }
}
