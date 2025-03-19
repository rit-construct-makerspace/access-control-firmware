/*

Access Control Sign-In Reader

This code is used to turn a V2.4.1 Light Edition Core from the ACS system into a room sign-in reader. 

For more information, visit: https://github.com/rit-construct-makerspace/access-control-firmware

Licensed CC-BY-NC-SA 4.0

Jim Heaney, RIT SHED Makerspace
make@rit.edu
March 2025

*/

//Settings:
#define Version "1.1.0"
#define Hardware "2.4.1-LE"
#define OTA_URL "https://github.com/rit-construct-makerspace/access-control-firmware/releases/latest/download/otadirectory.json"

//Pin Definitions
#define MOSI_PIN 8
#define MISO_PIN 11
#define SCK_PIN 10
#define CS_PIN 6

//Libraries:
#include <mfrc630.h>                //By Ivor Wanders, Source: https://github.com/iwanders/MFRC630
#include "USB.h"                    //Inherent to ESP32 Arduino

//Objects:
USBCDC USBSerial;

void setup() {
  // put your setup code here, to run once:

  // Start serial communication.
  USBSerial.begin();
  USB.begin();

  USBSerial.println("STARTUP");

  USBSerial.println(F("Loading Settings."));
  settings.begin("settings", false);
  SecurityCode = settings.getString("SecurityCode");
  if(SecurityCode == NULL){
    Serial.println(F("CAN'T FIND SETTINGS - FRESH INSTALL?"));
    Serial.println(F("HOLDING FOR UPDATE FOREVER..."));
    xTaskCreate(USBConfig, "USBConfig", 4096, NULL, 2, NULL);
    //Nuke the rest of this process - we can't do anything without our config.
    vTaskSuspend(NULL);
  }
  Password = settings.getString("Password");
  SSID = settings.getString("SSID");
  Server = settings.getString("Server");
  Key = settings.getString("Key");
  Zone = settings.getString("Zone");
  NoBuzzer = settings.getString("NoBuzzer").toInt();
  Brightness = settings.getString("Brightness").toInt();

  USBSerial.println(F("Starting LED"));

  LED.begin();
  LED.setBrightness(Brightness);
  LED.show();

  xTaskCreate(GamerMode, "GamerMode", 2048, NULL, 5, &xHandle2);

  USBSerial.println(F("Starting NFC Reader."));

  //Set Pin Modes.
  pinMode(CS_PIN, OUTPUT);

  //Start SPI
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);

  //Set the registers of the MFRC630 into the default.
  mfrc630_AN1102_recommended_registers(MFRC630_PROTO_ISO14443A_106_MILLER_MANCHESTER);

  //These are registers that are used in the MFRC630 example code, that seem to work for our application.
  //TODO: Figure out what these actually do, if we need them, if they should be different, etc. 
    mfrc630_write_reg(0x28, 0x8E);
  mfrc630_write_reg(0x29, 0x15);
  mfrc630_write_reg(0x2A, 0x11);
  mfrc630_write_reg(0x2B, 0x06);

  USBSerial.println(F("Starting WiFi."));

  USBSerial.print(F("Connecting to: ")); USBSerial.println(SSID);

  WiFi.mode(WIFI_STA);
  if(Password != "null"){
    WiFi.begin(SSID, Password);
  } else{
    USBSerial.println(F("Using no password."));
    WiFi.begin(SSID);
  }

  WiFi.setSleep(false);
  WiFi.setAutoRecconect(true);

  USBSerial.print(F("MAC Address: ")); USBSerial.println(WiFi.macAddress());

  while(WiFi.status() != WL_CONNECTED){
    USBSerial.print(".");
    delay(500);
  }
  USBSerial.println();

  USBSerial.print(F("Connected to ")); USBSerial.println(SSID);
  USBSerial.print(F("Local IP: ")); USBSerial.println(WiFi.localIP());

  client.setInsecure();
  http.setReuse(true);
  
  USBSerial.println(F("Checking for an OTA Update."));
  USBSerial.println(F("If any are found, will install immediately and restart."));

  ota.SetCallback(callback_percent);
  ota.SetConfig(Hardware);
  ota.OverrideDevice("Sign In");
  int otaresp = ota.CheckForOTAUpdate(OTA_URL, Version);
  USBSerial.print(F("OTA Response Code: ")); USBSerial.println(otaresp);
  USBSerial.println(errtext(otaresp));
  USBSerial.println(F("Looks like we're still here, must not have installed an OTA update."));

  //Disable the startup lights
  vTaskDelete(xHandle2);

  USBSerial.println(F("Starting FreeRTOS."));

  vTaskSuspendAll();
  xTaskCreate(USBConfig, "USBConfig", 2048, NULL, 5, NULL);
  xTaskCreate(SignIn, "SignIn", 6000, NULL, 5, NULL);
  vTaskResumeAll();

  USBSerial.println(F("Setup complete."));

}

void loop() {
  // put your main code here, to run repeatedly:
  //Don't need the loop, delete it.
  vTaskDelete(NULL);
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

void GamerMode(void *pvParameters){
  //This task simply blinks the front LED red-green-blue, aka gamer mode, to indicate startup in progress.
  while(1){
    Internal.println("L 255,0,0");
    delay(300);
    Internal.println("L 0,255,0");
    delay(300);
    Internal.println("L 0,0,255");
    delay(300);
  }
}
