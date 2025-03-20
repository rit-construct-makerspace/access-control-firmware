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
#define Version "1.2.0"
#define Hardware "2.4.1-LE"
#define OTA_URL "https://github.com/rit-construct-makerspace/access-control-firmware/releases/latest/download/otadirectory.json"
#define HIGH_TONE 2000
#define LOW_TONE 1500
#define ToneTime 250

//Pin Definitions
#define MOSI_PIN 8
#define MISO_PIN 11
#define SCK_PIN 10
#define CS_PIN 6
#define LED_PIN 1
#define Button 0
#define Buzzer 13

//Libraries:
#include <mfrc630.h>                //By Ivor Wanders, Source: https://github.com/iwanders/MFRC630
#include "USB.h"                    //Version 3.1.1 | Inherent to ESP32 Arduino
#include <Update.h>                 //Version 3.1.1 | Inherent to ESP32 Arduino
#include <WiFi.h>                   //Version 3.1.1 | Inherent to ESP32 Arduino
#include <ESP32OTAPull.h>           //Version 1.0.1 | Source: https://github.com/JimSHED/ESP32-OTA-Pull-GitHub
#include <WiFiClientSecure.h>       //Version 3.1.1 | Inherent to ESP32 Arduino
#include <HTTPClient.h>             //Version 3.1.1 | Inherent to ESP32 Arduino
#include <Preferences.h>            //Version 3.1.1 | Inherent to ESP32 Arduino
#include <Adafruit_NeoPixel.h>
#include <SPI.h>

//Variables:
String SecurityCode;                //The password needed to access the settings
String Password;                    //WiFi password
String SSID;                        //WiFi SSID
String Server;                      //Server to send API calls to
String Key;                         //API server key
String Zone;                        //Zone that user is makred as signed in to
bool NoBuzzer;                      //When 1, buzzer will not sound
int Brightness;                     //Brightness of the LEDs, from 0 (off) to 255 
byte ValidLength;                   //Optional parameter to check UID length to reject invalid NFC cards and the like. Set 0 to disable.
byte ValidSAK;                      //Optional parameter to check the NFC SAK to reject invalid NFC cards and the like. set to 0 to disable.
int ValidREQA;                      //Optional parameter to check the REQA to reject invalid NFC cards and the like. Set to 0 to disable.
uint64_t ResetTime;                 //Stores time when we should reset based on button.
bool Ready;                         //Indicates system in idle state, waiting for card.
bool InSystem;
bool NotInSystem;
bool InvalidCard;
bool NoNetwork;
byte NetworkError;                  //Increases by 1 every time there's a network issue, resets to 0 on successful network.
bool Fault;                         //1 to indicate system fault and set lights/buzzers properly.
bool BuzzerStart;
uint64_t NetworkTime;

//Objects:
USBCDC USBSerial;
TaskHandle_t xHandle2;
Preferences settings;
WiFiClientSecure client;
JsonDocument usbjson;
JsonDocument doc;
Adafruit_NeoPixel LED(1, LED_PIN, NEO_RGB + NEO_KHZ800);
ESP32OTAPull ota;
HTTPClient http;

void setup() {
  // put your setup code here, to run once:

  // Start USBSerial communication.
  USBSerial.begin();
  USB.begin();

  delay(1000);

  USBSerial.println("STARTUP");

  USBSerial.println(F("Loading Settings."));
  settings.begin("settings", false);
  SecurityCode = settings.getString("SecurityCode");
  if(SecurityCode == NULL){
    USBSerial.println(F("CAN'T FIND SETTINGS - FRESH INSTALL?"));
    USBSerial.println(F("HOLDING FOR UPDATE FOREVER..."));
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
  ValidLength = settings.getString("ValidLength").toInt();
  ValidSAK = settings.getString("ValidSAK").toInt();
  ValidREQA = settings.getString("ValidREQA").toInt();

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
  WiFi.setAutoReconnect(true);

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
  xTaskCreate(BuzzerControl, "BuzzerControl", 2048, NULL, 6, NULL);
  xTaskCreate(LEDControl, "LEDControl", 2048, NULL, 6, NULL);
  xTaskResumeAll();

  pinMode(Button, INPUT);

  USBSerial.println(F("Setup complete."));

}

void loop() {
  // put your main code here, to run repeatedly:
  // The loop just monitors for a reset
  if(digitalRead(Button)){
    ResetTime = millis64() + 3000;
  }
  if(ResetTime <= millis64()){
    LEDColor(255,0,0);
    USBSerial.println(F("Restarting Device..."));
    USBSerial.flush();
    delay(1000);
    ESP.restart();
    
  }

}

void callback_percent(int offset, int totallength) {
  //Used to display percentage of OTA installation
  static int prev_percent = -1;
  int percent = 100 * offset / totallength;
  if (percent != prev_percent) {
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

void GamerMode(void *pvParameters){
  //This task simply blinks the front LED red-green-blue, aka gamer mode, to indicate startup in progress.
  while(1){
    LEDColor(255,0,0);
    delay(300);
    LEDColor(0,255,0);
    delay(300);
    LEDColor(0,0,255);
    delay(300);
  }
}

void LEDColor(byte Red, byte Green, byte Blue){
  //Simple function to set the LED color and show it immediately
  LED.setPixelColor(0, Red, Green, Blue);
  LED.show();
}

uint64_t millis64(){
  //This simple function replaces the 32 bit default millis. Means that overflow now occurs in 290,000 years instead of 50 days
  //Timer runs in microseocnds, so divide by 1000 to get millis.
  return esp_timer_get_time() / 1000;
}

//The following are from the MFRC630 library.

//Implement the HAL functions on an Arduino compatible system.
void mfrc630_SPI_transfer(const uint8_t* tx, uint8_t* rx, uint16_t len) {
  for (uint16_t i=0; i < len; i++){
    rx[i] = SPI.transfer(tx[i]);
  }
}

//Select the chip and start an SPI transaction.
void mfrc630_SPI_select() {
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));  // gain control of SPI bus
  digitalWrite(CS_PIN, LOW);
}

//Unselect the chip and end the transaction.
void mfrc630_SPI_unselect() {
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();    // release the SPI bus
}

//Hex print for blocks without printf.
void print_block(uint8_t * block,uint8_t length){
    for (uint8_t i=0; i<length; i++){
        if (block[i] < 16){
          USBSerial.print("0");
          USBSerial.print(block[i], HEX);
        } else {
          USBSerial.print(block[i], HEX);
        }
        USBSerial.print(" ");
    }
}