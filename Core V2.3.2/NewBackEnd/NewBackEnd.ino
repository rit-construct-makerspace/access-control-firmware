//Re-write of ACS Core firmware to make it simpler, faster. Uses just one mega loop for everything.

#define Version "2.0.0"
#define Hardware "2.3.2-LE"
#define DebugMode 1

//Pin Definitions:
  const int ETHINT = 13;
  const int DB9INT = 37;
  const int ETHLEDS = 40;
  const int SWTYPE = 2;
  const int TEMP = 1;
  const int DIP3 = 12;
  const int DIP2 = 11;
  const int DIP1 = 10;
  const int TOTINY = 9;
  const int ETHCS = 3;
  const int ETHRST = 20;
  const int TOESP = 19;
  const int NFCPWR = 8;
  const int SCKPin = 17;
  const int MISOPin = 16;
  const int DEBUGLED = 15;
  const int MOSIPin = 7;
  const int NFCCS = 6;
  const int NFCIRQ = 5;
  const int NFCRST = 4;

//Libraries:
  #include <OneWire.h>              //Replacing for WSACS API update...
  #include <Adafruit_PN532.h>       //Version 1.3.4 | Source: https://github.com/adafruit/Adafruit-PN532
  #include <ArduinoJson.h>          //Version 7.3.0 | Source: https://github.com/bblanchon/ArduinoJson
  #include <ArduinoJson.hpp>        //Version 7.3.0 | Source: https://github.com/bblanchon/ArduinoJson
  //WARNING: The original ESP32-OTA-Pull will not work with this code. Use the forked version linked below!
  #include <ESP32OTAPull.h>         //Version 1.0.1 | Source: https://github.com/JimSHED/ESP32-OTA-Pull-GitHub
  #include <WiFiClientSecure.h>     //Version 3.1.1 | Inherent to ESP32 Arduino
  #include <HTTPClient.h>           //Version 3.1.1 | Inherent to ESP32 Arduino
  #include <Preferences.h>          //Version 3.1.1 | Inherent to ESP32 Arduino
  #include <esp_wifi.h>             //Version 3.1.1 | Inherent to ESP32 Arduino
  #include <FS.h>                   //Version 3.1.1 | Inherent to ESP32 Arduino
  #include <SPIFFS.h>               //Version 3.1.1 | Inherent to ESP32 Arduino
  #include <Update.h>               //Version 3.1.1 | Inherent to ESP32 Arduino
  #include <WiFi.h>                 //Version 3.1.1 | Inherent to ESP32 Arduino
  #include "esp_timer.h"            //Version 3.1.1 | Inherent to ESP32 Arduino
  #include "esp32s2/rom/rtc.h"      //Version 3.1.1 | Inherent to ESP32 Arduino
  #include <nvs_flash.h>            //Version 3.1.1 | Inherent to ESP32 Arduino
  #include <ESP32Time.h>            //Version 2.0.6 | Source: https://github.com/fbiego/ESP32Time
  #include <WebSocketsClient.h>     //Version 2.6.1 | Source: https://github.com/Links2004/arduinoWebSockets
  #include <ESP32Ping.h>            //Version 1.6   | Source: https://github.com/marian-craciunescu/ESP32Ping
  #include <ping.h>                 //Version 1.6   | Source: https://github.com/marian-craciunescu/ESP32Ping
  #include "esp_ota_ops.h"          //Version 3.1.1 | Inherent to ESP32 Arduino

//Objects:
  Adafruit_PN532 nfc(SCKPin, MISOPin, MOSIPin, NFCCS);
  Preferences settings;
  WiFiClientSecure client;
  HardwareSerial Internal(1);
  JsonDocument usbjson;
  HTTPClient http;
  ESP32OTAPull ota;
  ESP32Time rtc;
  WebSocketsClient socket;

extern "C" bool verifyRollbackLater() {
  //This code is run to verify the OTA before actual setup.
  //Since we are handling OTA verification ourselves, we just return true.
  return true;
}

//SSL Certificate. R12 for make.rit.edu running on Let's Encrypt. Will expire on March 12 2027!
const char *root_ca = R"literal(
-----BEGIN CERTIFICATE-----
MIIFBjCCAu6gAwIBAgIRAMISMktwqbSRcdxA9+KFJjwwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjQwMzEzMDAwMDAw
WhcNMjcwMzEyMjM1OTU5WjAzMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg
RW5jcnlwdDEMMAoGA1UEAxMDUjEyMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB
CgKCAQEA2pgodK2+lP474B7i5Ut1qywSf+2nAzJ+Npfs6DGPpRONC5kuHs0BUT1M
5ShuCVUxqqUiXXL0LQfCTUA83wEjuXg39RplMjTmhnGdBO+ECFu9AhqZ66YBAJpz
kG2Pogeg0JfT2kVhgTU9FPnEwF9q3AuWGrCf4yrqvSrWmMebcas7dA8827JgvlpL
Thjp2ypzXIlhZZ7+7Tymy05v5J75AEaz/xlNKmOzjmbGGIVwx1Blbzt05UiDDwhY
XS0jnV6j/ujbAKHS9OMZTfLuevYnnuXNnC2i8n+cF63vEzc50bTILEHWhsDp7CH4
WRt/uTp8n1wBnWIEwii9Cq08yhDsGwIDAQABo4H4MIH1MA4GA1UdDwEB/wQEAwIB
hjAdBgNVHSUEFjAUBggrBgEFBQcDAgYIKwYBBQUHAwEwEgYDVR0TAQH/BAgwBgEB
/wIBADAdBgNVHQ4EFgQUALUp8i2ObzHom0yteD763OkM0dIwHwYDVR0jBBgwFoAU
ebRZ5nu25eQBc4AIiMgaWPbpm24wMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzAC
hhZodHRwOi8veDEuaS5sZW5jci5vcmcvMBMGA1UdIAQMMAowCAYGZ4EMAQIBMCcG
A1UdHwQgMB4wHKAaoBiGFmh0dHA6Ly94MS5jLmxlbmNyLm9yZy8wDQYJKoZIhvcN
AQELBQADggIBAI910AnPanZIZTKS3rVEyIV29BWEjAK/duuz8eL5boSoVpHhkkv3
4eoAeEiPdZLj5EZ7G2ArIK+gzhTlRQ1q4FKGpPPaFBSpqV/xbUb5UlAXQOnkHn3m
FVj+qYv87/WeY+Bm4sN3Ox8BhyaU7UAQ3LeZ7N1X01xxQe4wIAAE3JVLUCiHmZL+
qoCUtgYIFPgcg350QMUIWgxPXNGEncT921ne7nluI02V8pLUmClqXOsCwULw+PVO
ZCB7qOMxxMBoCUeL2Ll4oMpOSr5pJCpLN3tRA2s6P1KLs9TSrVhOk+7LX28NMUlI
usQ/nxLJID0RhAeFtPjyOCOscQBA53+NRjSCak7P4A5jX7ppmkcJECL+S0i3kXVU
y5Me5BbrU8973jZNv/ax6+ZK6TM8jWmimL6of6OrX7ZU6E2WqazzsFrLG3o2kySb
zlhSgJ81Cl4tv3SbYiYXnJExKQvzf83DYotox3f0fwv7xln1A2ZLplCb0O+l/AK0
YE0DS2FPxSAHi0iwMfW2nNHJrXcY3LLHD77gRgje4Eveubi2xxa+Nmk/hmhLdIET
iVDFanoCrMVIpQ59XWHkzdFmoHXHBV7oibVjGSO7ULSQ7MJ1Nz51phuDJSgAIU7A
0zrLnOrAj/dfrlEWRhCvAgbuwLZX1A2sjNjXoPOHbsPiy+lO1KF8/XY7
-----END CERTIFICATE-----
)literal";

//Variables - Inter-Task Communication
bool GamerMode = 1;  //Set to 0 to disable gamer mode, i.e. cycle RGB. Used during boot.
bool RequestReset = 0; //Other tasks can set this to 1 to tell the ResetController to start a restart. 
String ResetReason; //Tells the RestartController why we are restarting.
bool Access = 0; //Used to tell the frontend to enable the access signal to the bus. 
String UID; //Stores the UID of the user currently using the machine.
bool ChangeBeep = 0; //Lets the frontend know to beep.

//Variables - System State
bool Identify = 0; //Set to 1 to play an identification alarm/buzzer.
String State = "UNKNOWN"; //State of the machine, uses the WSACS API standard wording (IDLE, UNLOCKED, ALWAYS_ON, LOCKED_OUT, FAULT, UNKNOWN)
bool PendingApproval = 0; //Set to 1 when we have a card present that hasn't been authed yet, this is used for LED animations. 
bool AccessDenied = 0; //Set to 1 when a card is present but has been denied, for LED animations. 
bool CardPresent = 0; //Used to track if there is a card present in the machine.
bool NFCBroken = 0;   //Set to 1 if we lose the NFC reader, needed since the NFC reader is an external component on these boards. 
String LastState; //Stores what the state was previously, to detect changes.

//Variables - Config
String SerialNumber;
String Password;
String SSID;
String Server;
String Key;

//Variables - WebSocket
bool JustDisconnected = 1; //Stops spamming of websocket disconnect messages.
bool WSSend = 0;     //Tracks if we have something to send
unsigned long long NextPing = 0; //The time to send the next ping. A ping goes out every 2 seconds.
String WSIncoming;   //String of incoming data from server;
bool NewFromWS; //Set to 1 if there is data to be read in WSIncoming.

void setup() {
  // put your setup code here, to run once:

  //Connect to the frontend, set the buzzer off. In case we crashed while we were writing the buzzer.
  Internal.begin(115200, SERIAL_8N1, TOESP, TOTINY);
  Internal.println("B 0");
  delay(50);
  Internal.flush();
  Internal.println("B 0");
  Internal.println("L 255,0,0"); //Set it solid red until the task takes over. 

  //Create tasks to handle the internal reading, writing to the frontend.
  xTaskCreate(InternalSerial, "InternalSerial", 1024, NULL, 5, NULL);
  xTaskCreate(AVControl, "AVControl", 1024, NULL, 5, NULL);
  xTaskCreate(RestartController, "RestartController", 1024, NULL, 5, NULL);

  //Load settings from memory
  settings.begin("settings", false);
  Server = settings.getString("Server");
  if(Server == NULL){
    //We don't have a valid config? Flash blue forever. 
    //We can't do anything with this to deploy new hardware
    while(1){
      Internal.println("L 0,0,0");
      delay(1000);
      Internal.println("L 0,0,255");
      delay(1000);
    }
  }
  SerialNumber = settings.getString("SerialNumber");
  Password = settings.getString("Password");
  if(Password.equalsIgnoreCase("null")){
    //Use a real NULL password.
    Password = "";
  }
  SSID = settings.getString("SSID");
  Server = settings.getString("Server");
  Key = settings.getString("Key");
  int TimezoneHr;
  if(settings.isKey("Timezone")){
    TimezoneHr = settings.getString("Timezone").toInt();
  } else{
    TimezoneHr = -4;
  }
  rtc.offset = TimezoneHr * 3600;

  //Connect to the network before we continue.
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, Password);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  //If we cared about why we restarted, this'd be the place to handle it.

  //Check for an OTA update, install it if there is one present.
  ota.SetCallback(callback_percent);
  ota.SetConfig(Hardware);
  ota.OverrideDevice("ACS Core");
  ota.EnableSerialDebug();
  int otaresp = ota.CheckForOTAUpdate(OTA_URL, Version);
  Serial.print(F("OTA Response: "));
  Serial.println(errtext(otaresp));

  //If we made it past the OTA, then we are ready for normal operation.

  //Start the NFC reader, make sure it is present.
   uint32_t versiondata;
  pinMode(NFCPWR, OUTPUT);
  pinMode(NFCRST, OUTPUT);
  digitalWrite(NFCPWR, HIGH);
  digitalWrite(NFCRST, HIGH);
  delay(100);
  while(1){
    nfc.begin();
    Serial.println(F("Looking for NFC Reader."));
    versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
      delay(1000);
      Serial.println(F("Didn't find NFC reader. Retrying..."));
    } else{
      Serial.println(F("NFC reader reports OK!"));
      break;
    }
  }

  //We should initialize the OneWire bus here, check for the right devices, etc.
  Serial.println(F("Starting OneWire..."));
  loadInventoryFromFile();
  if(deviceCount == 0){
     Serial.println(F("Inventory empty! Scanning to initialize..."));
     discoverDevices(); // This sets the initial baseline
     saveInventoryToFile(); // Save it so it's not empty next time
  }
  //We should also immediately do a OneWire integrity check.
  Serial.println(F("Checking Bus Integrity..."));
  checkBusHealth();
  updateBusTemperatures();
  refreshLiveAddressBuffer();

  //Going forward, we will check the OneWire bus in a different task to make life easier.
  xTaskCreate(BusManager, "BusManager", 2048, NULL, 5, NULL);

  //Start our websocket connection
  //New API requires some info in the header of the Websocket starting:
  String ExtraHeader = "device-sn:" + SerialNumber + "\r\ndevice-key:" + Key;
  socket.setExtraHeaders(ExtraHeader.c_str());
  //socket.begin(Server.c_str(), 80, "/api/devices/cores/access/ws");
  socket.beginSslWithCA(Server.c_str(), 443, "/api/devices/cores/access/ws", root_ca);
  Serial.print(F("Attempting initial connect to Websocket..."));
  while(!socket.isConnected()){
    Serial.print(".")
  }
  Serial.println();
  Serial.println(F("Websocket connected!"));
  socket.onEvent(webSocketEvent);
  socket.setReconnectInterval(2000); //Attempt to reconnect every 2 seconds if we lose connection

  //Time to loop! 
  GamerMode = 0; //Disable the startup lighting

}

void loop() {
  // put your main code here, to run repeatedly:

  //Step 1: Check machine state variables

  //Step 1.1: Check for any reason we should be in a fault state
  if(OverTemp || SealBroken || NFCBroken){
    State = "FAULT";
  }

  //Step 1.2: Check for a change to the card. New one? Removed? 
  //Was a new card inserted?
  if(!CardPresent && Switch1 && Switch2){
    CardPresent = 1;
    //Let's read the card;
    byte NFCRetryCount = 0;
    bool success;                                //Determines if an NFC read was successful
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;                           // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    while(NFCRetryCount <= 5){
      //Power cycle the NFC reader
      digitalWrite(NFCPWR, HIGH);
      vTaskDelay(10 / portTICK_PERIOD_MS);
      digitalWrite(NFCPWR, LOW);
      vTaskDelay(10 / portTICK_PERIOD_MS);
      nfc.wakeup();
      nfc.setPassiveActivationRetries(0xFF);
      //Attempt to read the card via NFC:
      success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength, 500);
      if(success){
        break;
      } else{
        NFCRetryCount++;
      }
    }
    if(NFCRectryCount > 5){
      //We failed to read the NFC card all 5 times.
      //Is there actually an NFC card here?
      uint32_t versiondata = nfc.getFirmwareVersion();
      if(!versiondata){
        //This means the NFC reader is broken
        NFCBroken = true;
        Serial.println(F("NFC Reader Broken?"));
        continue;
      } else{
        //Reader is working, must not be an NFC card?
        AccessDenied = true;
        Serial.println(F("Not an NFC card?"));
        continue;
      }
    }
    UID = "";
    Serial.print(F("Card: "));
    for (uint8_t i = 0; i < uidLength; i++) {
      Serial.print(uid[i], HEX);
      Serial.print(" ");
      char buf[3] = {0};
      snprintf(buf, sizeof(buf), "%02x", uid[i]);
      UID += String(buf);
    }
    Serial.println(" Found.");
    if(State == "IDLE"){
      //Let's check with the server
      PendingApproval = true;
    } else if(State == "UNLOCKED" || State == "ALWAYS_ON"){
      //Nothing changes, but let's give a beep to the user.
      ChangeBeep = true;
    } else{
      //Auto-deny the user
      AccessDenied = true;
    }
  }
  //Was a card that was present removed?
  if(CardPresent && (!Switch1 || !Switch2){
    //Reset everything to normal.
    CardPresent = false;
    UID = "";
    PendingApproval = false;
    AccessDenied = false;
    if(State == "UNLOCKED"){
      State = "IDLE";
    }
  }

  //Step 1.3: Check if the state has changed since last time we went through the loop.
  if(State != LastState){

  }

  //Step 4: Communicate with the server
  //Step 4.1: See if we have any outgoing messages, and send them.
  //Step 4.2: Check for incoming messages and handle. If we got a message, mark the OTA as secure.
  //Step 4.3: Send handle the ping-pong.

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

uint64_t millis64(){
  //This simple function replaces the 32 bit default millis. Means that overflow now occurs in 290,000 years instead of 50 days
  //Timer runs in microseocnds, so divide by 1000 to get millis.
  return esp_timer_get_time() / 1000;
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  //Handles all incoming Websocket-related stuff
  //We check for OTA validity on the receipt of a text message. Need this info to do that;
  switch(type) {
    case WStype_DISCONNECTED:
      if(JustDisconnected){
        //Stops spam if we have no connection
        Serial.printf("Websocket Reports Disconnected.");
        JustDisconnected = 0;
      }
      WSSend = 0; //Clear out anything that needs to be sent
      break;
    case WStype_CONNECTED:
      JustDisconnected = 1;
      Serial.println(F("Websocket Reports Connected."));
      NextPing = millis64() + 2000;
      break;
    case WStype_TEXT:
      WSIncoming = String((char *)payload, length);
      NewFromWS = 1;
      NextPing = millis64() + 2000;
      break;
    case WStype_PONG:
      //Serial.println(F("PONG!"));
      NextPing = millis64() + 2000;
      break;
    case WStype_ERROR:
      Serial.println(F("Got a websocket error!"));
      break;
  }
}