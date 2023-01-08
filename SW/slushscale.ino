/*-----------------------------------------------------------
Code by Deadeye5589
Rev 0.2
Date 08.01.2023
-----------------------------------------------------------*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <String.h>
#include "time_ntp.h"
#include "HX711.h"

bool wm_nonblocking = false;
WiFiManager wm;                       // global wm instance
WiFiManagerParameter serverURL;       // parameter to store CW Web Server URL where JSON object will be send to
WiFiManagerParameter NTP_URL;         // IP of the NTP Server we want to request date and time from 

// HX711 circuit wiring
const int LOADCELL0_DOUT_PIN = 0;
const int LOADCELL0_SCK_PIN = 2;
const int LOADCELL1_DOUT_PIN = 4;
const int LOADCELL1_SCK_PIN = 16;
const int LOADCELL2_DOUT_PIN = 17;
const int LOADCELL2_SCK_PIN = 5;
const int LOADCELL3_DOUT_PIN = 18;
const int LOADCELL3_SCK_PIN = 19;

HX711 scale0;
HX711 scale1;
HX711 scale2;
HX711 scale3;

long messwertewaage[4];

//Your Domain name with URL path or IP address with path
const char* serverName = "default";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

void setup() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP 
  Serial.begin(115200);
  Serial.setDebugOutput(true);  
  delay(3000);
  Serial.println("\n Starting");
  Serial.println(serverName);

  //wm.resetSettings(); // wipe settings

  if(wm_nonblocking) wm.setConfigPortalBlocking(false);

  // add a custom input field
  int customFieldLength = 80;

  new (&serverURL) WiFiManagerParameter("serverURL", "CW Web Server URL", "https://10.10.12.137:7041/cwweb/api/v1/slush_machines/measurements", customFieldLength,"placeholder=\"https://10.10.12.137:7041/cwweb/api/v1/slush_machines/measurements\"");
  new (&NTP_URL) WiFiManagerParameter("NTP_URL", "NTP Server URL (use comma)", "134,130,4,17", customFieldLength,"placeholder=\"134,130,4,17\"");
  
  wm.addParameter(&serverURL);
  wm.addParameter(&NTP_URL);
  wm.setSaveParamsCallback(saveParamCallback);

  std::vector<const char *> menu = {"wifi","info","param","sep","restart","exit"};
  wm.setMenu(menu);

  // set dark theme
  wm.setClass("invert");

  wm.setConnectTimeout(20); // how long to try to connect for before continuing

  bool res;
  res = wm.autoConnect("AutoConnectAP","campuswoche"); // password protected ap

  if(!res) {
    Serial.println("Failed to connect or hit timeout");
    // ESP.restart();
  } 
  else {
    //if you get here you have connected to the WiFi    
    Serial.println("connected...yeey :)");
  }


  Serial.println("Setup sync with NTP service.");
  setSyncProvider(getNTP_UTCTime1970);
  setSyncInterval(86400); // NTP re-sync; i.e. 86400 sec would be once per day
  ntptimetostring();

  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");

  scale0.begin(LOADCELL0_DOUT_PIN, LOADCELL0_SCK_PIN);
  messwertewaage[0] = scale0.read();
  scale0.set_scale(2280.f);    // this value is obtained by calibrating the scale with known weights; see the README for details
  scale0.tare();				        // reset the scale to 0

  scale1.begin(LOADCELL1_DOUT_PIN, LOADCELL1_SCK_PIN);
  messwertewaage[1] = scale1.read();
  scale1.set_scale(2280.f);    // this value is obtained by calibrating the scale with known weights; see the README for details
  scale1.tare();				        // reset the scale to 0

  scale2.begin(LOADCELL2_DOUT_PIN, LOADCELL2_SCK_PIN);
  messwertewaage[2] = scale2.read();
  scale2.set_scale(2280.f);    // this value is obtained by calibrating the scale with known weights; see the README for details
  scale2.tare();				        // reset the scale to 0

  scale3.begin(LOADCELL3_DOUT_PIN, LOADCELL3_SCK_PIN);
  messwertewaage[3] = scale3.read();
  scale3.set_scale(2280.f);    // this value is obtained by calibrating the scale with known weights; see the README for details
  scale3.tare();				        // reset the scale to 0
}


String getParam(String name){
  //read parameter from server, for customhmtl input
  String value;
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback(){
  Serial.println("[CALLBACK] saveParamCallback fired");
  Serial.println("PARAM CW Web Server URL = " + getParam("serverURL"));
  serverName = getParam("serverURL").c_str();
  Serial.println("PARAM NTP Server URL = " + getParam("NTP_URL"));
}


void loop() {
  if(wm_nonblocking) wm.process(); // avoid delays() in loop when non-blocking and other long running code  
  //Send an HTTP POST request every 10 minutes
  if ((millis() - lastTime) > timerDelay) {
    //Check WiFi connection status
    if(WiFi.status()== WL_CONNECTED){
      ntptimetostring();

      if (scale0.is_ready()) {
        messwertewaage[0] = scale0.get_units(5);
        messwertewaage[1] = scale1.get_units(5);
        messwertewaage[2] = scale2.get_units(5);
        messwertewaage[3] = scale3.get_units(5);
        Serial.println("Messe Gewichte: ");
        for(int i=0; i<4; i++)
          Serial.println(messwertewaage[i]);
      } else {
        Serial.println("HX711 not found.");
      }

      WiFiClient client;
      HTTPClient http;
      
      // Your Domain name with URL path or IP address with path
      http.begin(client, serverName);

      // Specify content-type header
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      
      // If you need an HTTP request with a content type: application/json, use the following:
      http.addHeader("Content-Type", "application/json");
      int httpResponseCode = http.POST("{ \"timestamp\": \"" + String(timebuffer) +"\", \"points\": ["+ messwertewaage[0] + "," + messwertewaage[1] + "," + messwertewaage[2] + "," + messwertewaage[3] + "] }");
   
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
