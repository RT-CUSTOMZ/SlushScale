/*-----------------------------------------------------------
Code by Deadeye5589
Rev 0.1
Date 21.12.2022
-----------------------------------------------------------*/

#include <WiFi.h>
#include <HTTPClient.h>
#include "time_ntp.h"


const char* ssid = "xxxx";
const char* password = "xxxx";

long zufall[4];

//Your Domain name with URL path or IP address with path
const char* serverName = "https://10.10.12.137:7041/cwweb/api/v1/slush_machines/measurements";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 5000;

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
 
  Serial.println("Setup sync with NTP service.");
  setSyncProvider(getNTP_UTCTime1970);
  setSyncInterval(86400); // NTP re-sync; i.e. 86400 sec would be once per day
  ntptimetostring();

  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");
}

void loop() {
  //Send an HTTP POST request every 10 minutes
  if ((millis() - lastTime) > timerDelay) {
    //Check WiFi connection status
    if(WiFi.status()== WL_CONNECTED){
      ntptimetostring();

      randomSeed(42);
      for(int i=0; i<4; i++){
        zufall[i] = random(0,15000);
        Serial.println(zufall[i]);
      }

      WiFiClient client;
      HTTPClient http;
    
      // Your Domain name with URL path or IP address with path
      http.begin(client, serverName);

      // Specify content-type header
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      
      // If you need an HTTP request with a content type: application/json, use the following:
      http.addHeader("Content-Type", "application/json");
      int httpResponseCode = http.POST("{ \"timestamp\": \"" + String(timebuffer) +"\", \"points\": ["+ zufall[0] + "," + zufall[1] + "," + zufall[2] + "," + zufall[3] + "] }");
   
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
        
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }
}
