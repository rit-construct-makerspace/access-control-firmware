/*

V1.1.0 Updater

This quick program will load all the required settings to transition from 1.1.0 to 1.2.0 and above firmware, and automatically install it via OTA.

*/

//Libraries:
#include <ArduinoJson.h>          //Version 7.3.0 | Source: https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.hpp>        //Version 7.3.0 | Source: https://github.com/bblanchon/ArduinoJson
//WARNING: The original ESP32-OTA-Pull will not work with this code. Use the forked version linked below!
#include <ESP32OTAPull.h>         //Version 1.0.1 | Source: https://github.com/JimSHED/ESP32-OTA-Pull-GitHub
#include <WiFiClientSecure.h>     //Version 3.1.1 | Inherent to ESP32 Arduino
#include <HTTPClient.h>           //Version 3.1.1 | Inherent to ESP32 Arduino
#include <Preferences.h>          //Version 3.1.1 | Inherent to ESP32 Arduino
#include <esp_wifi.h>             //Version 3.1.1 | Inherent to ESP32 Arduino
#include <time.h>                 //Version 3.1.1 | Inheren`t to ESP32 Arduino
#include <Update.h>               //Version 3.1.1 | Inherent to ESP32 Arduino
#include <WiFi.h>                 //Version 3.1.1 | Inherent to ESP32 Arduino

bool DebugMode = 1;

#define OTA_URL "https://github.com/rit-construct-makerspace/access-control-firmware/releases/latest/download/otadirectory.json"
#define Version "0.0.0" //Forces update
#define Hardware "2.3.2-LE"

//Objects:
Preferences settings;
WiFiClientSecure client;
JsonDocument usbjson;
HTTPClient http;

void setup() {
  // put your setup code here, to run once:

	Serial.begin(115200);
  Serial.println(F("STARTUP"));

  settings.begin("settings", false);
  String Password = settings.getString("Password");
  String SSID = settings.getString("SSID");

  //Add the missing settings
  UpdateSetting("DebugMode","0");
  UpdateSetting("SecurityCode","Shlug");
  UpdateSetting("NeedsWelcome","1");

  //Wireless Initialization:
  if (Password != "null") {
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, Password);
  } else {
    if (DebugMode) {
      Serial.println(F("Using no password."));
    }
    WiFi.begin(SSID);
  }

  //Attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    //Add another dot every second...
    delay(1000);
  }
  if (DebugMode) {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(SSID);
    Serial.print(F("Local IP: "));
    Serial.println(WiFi.localIP());
  }

  client.setInsecure();

  //Then, check for an OTA update.
  if(DebugMode){
    Serial.println(F("Checking for OTA Updates..."));
    Serial.println(F("If any are found, will install immediately."));
  }
  ESP32OTAPull ota;
  ota.EnableSerialDebug();
  ota.SetCallback(callback_percent);
  ota.SetConfig(Hardware);
  int otaresp = ota.CheckForOTAUpdate(OTA_URL, Version);
  if(DebugMode){
    Serial.print(F("OTA Response Code: ")); Serial.println(otaresp);
    Serial.println(errtext(otaresp));
    Serial.println(F("We're still here, so there must not have been an update."));
  }
  ESP.restart();
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(1000);
}

void UpdateSetting(String Key, String Value) { 
  //Updates a value in the preferences with a value from a JSON document.
  const char* Temp = Value.c_str();
  const char* KeyArray = Key.c_str();
  if(DebugMode){
    Serial.print(F("Updating key "));
    Serial.print(KeyArray);
    Serial.print(F(" with value "));
    Serial.println(Temp);
  }
  //Key is present
  settings.putString(Key.c_str(), Temp);
}

void callback_percent(int offset, int totallength) {
  //Used to display percentage of OTA installation
  static int prev_percent = -1;
  int percent = 100 * offset / totallength;
  if (percent != prev_percent && DebugMode) {
    Serial.printf("Updating %d of %d (%02d%%)...\n", offset, totallength, 100 * offset / totallength);
    prev_percent = percent;
  }
}

const char *errtext(int code) {
  //Deciphers OTA code response
  switch (code) {
    case ESP32OTAPull::UPDATE_AVAILABLE:
      return "An update is available but wasn't installed";
    case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND:
      return "No profile matches";
    case ESP32OTAPull::NO_UPDATE_AVAILABLE:
      return "Profile matched, but update not applicable";
    case ESP32OTAPull::UPDATE_OK:
      return "An update was done, but no reboot";
    case ESP32OTAPull::HTTP_FAILED:
      return "HTTP GET failure";
    case ESP32OTAPull::WRITE_ERROR:
      return "Write error";
    case ESP32OTAPull::JSON_PROBLEM:
      return "Invalid JSON";
    case ESP32OTAPull::OTA_UPDATE_FAIL:
      return "Update fail (no OTA partition?)";
    default:
      if (code > 0)
        return "Unexpected HTTP response code";
      break;
  }
  return "Unknown error";
}