/*

----- Access Control Core ----

For hardware version 2.3.2

By Jim Heaney, RIT SHED Makerspace
February 2025

Licensed CC-BY-NC-SA 4.0

More information; https://github.com/rit-construct-makerspace/access-control-firmware

------

FUNCTIONS:
InternalWrite: Handles writing updated information to the frontend MCU
InternalRead: Handles reading incoming messages from the frontend MCU
ReadCard: Manages the NFC readers
VerifyID: Checks if an installed ID is permitted to use this equipment
Temperature: Monitors all temperature sensors for both temperature as well as unique device IDs
LEDControl: Sets the LED color based on system state, handles animations
BuzzerControl: Sets the buzzer based on system state, handles tone sequences
MachineState: Handles the state of the machine and activations based on input flags
USBConfig: Allows programatic changing of settings over USB

*/

//Settings
#define Version "1.4.0"
#define Hardware "2.3.2-LE"
#define MAX_DEVICES 5 //How many possible temperature sensors to scan for
#define OTA_URL "https://raw.githubusercontent.com/rit-construct-makerspace/access-control-firmware/refs/heads/main/otadirectory.json"
#define CONFIG_APP_ROLLBACK_ENABLE
#define TemperatureTime 5000 //How long to delay between temperature measurements, in milliseconds
#define FEPollRate 10000 //How long, in milliseconds, to go between an all-values poll of the frontend (in addition to event-based)
#define LEDFlashTime 400 //Time in milliseconds between aniimation steps of the LED when flashing or similar. Set to 400 (2.5Hz) for epilepsy safety
#define LEDBlinkTime 3000 //Time in milliseconds between animation stepf of an LED when doing a slower blink indication.
#define BuzzerNoteTime 250 //Time in milliseconds between different tones
#define KEYSWITCH_DEBOUNCE 150 //time in milliseconds between checks of the key switch, to help prevent rapid state changes.
#define InternalReadDebug 0 //Set to 0 to disable debug messages from the internal read, since it ends up spamming the terminal.
#define DumpKey 0 //Set to 1 to allow the API key to be dumped over USB when requested. If set to 0, will just return "super-secret"
#define SanitizeDebug 1 //Set to 1 to remove the key when printing debug information. With this and DumpKey, there is no way to pull the API key without uploading malicious code.
#define BAD_INPUT_THRESHOLD 5 //If the wrong password or a bad JSON is loaded more than this many times, delete all information as a safety.
#define TXINTERRUPT 0 //Set to 1 to route UART0 TX to the DB9 interrupt pin, to allow external loggers to capture crash data.
//#define WebsocketUART //Uncomment to get messages from uart as if it is a websocket for testing. Also disables USB config to prevent issues there.
#define DebugMode 0 //Set to 1 for verbose output via UART, /!\ WARNING /!\ can dump sensitive information

//Global Variables:
bool TemperatureUpdate;                  //1 when writing new information, to indicate other devices shouldn't read temperature-related info
uint64_t SerialNumbers[MAX_DEVICES];     //an array of uint64_t serial numbers of the devices. Size of the array is based on MAX_DEVICES 
char devices;                            //How many onewire devices were detected, and therefore how many hardware components make up the ACS deployment
float SysMaxTemp;                        //a float of the maximum temperature of the system
bool DebugPrinting;                      //1 when a thread is writing on debug serial
byte NetworkMode = 1;                    //0 means WiFi only, 1 means WiFi or Ethernet, 2 means Ethernet only. TODO ethernet not implemented
String SSID = "";                        //The SSID that the wireless network will connect to.
String Password = "";                    //Stores the WiFi password
String Server;                           //The server address that API calls are made against
String Key;                              //Stores the API access key
int TempLimit;                           //The temperature above which the system shuts down and sends a warning.
int Frequency;                           //How often an update should be sent
bool Button;                             //state of the frontend button.
bool Switch1;                            //State of card detect switch 1
bool Switch2;                            //state of card detect switch 2
bool Key1;                               //state of keyswitch input 1
bool Key2;                               //state of keyswitch input 2
String UID;                              //UID of the card detected in the system
bool CardPresent;                        //1 if a card has been detected and successfuly read.
bool ReadFailed;                         //set to 1 if a card was not read properly.
bool WritingMessage;                     //set to 1 to indicate a message is being written, for eventual sending to the server.
String Message;                          //String containing error message to report via API
bool ReadyToSend;                        //set to 1 to indicate "Message" is ready to send.
String LogKey;                           //String containing a key to be reported via log API
String LogValue;                           //String containing a key to be reported via log API
bool LogReadyToSend;                     //set to 1 to indicate "Log" is ready to send.
bool RegularStatus;                      //Used to indicate it is time to send the regular status message.
bool TemperatureStatus;                  //Set to 1 to indicate to send a temperature status report.
bool StartupStatus;                      //Set to 1 to indicate to send a report that the system has powered on and completed startup..
bool StartStatus;                        //Set to 1 to indicate to send a report that a session has started (card inserted and approved).
bool EndStatus;                          //Set to 1 to indicate to send a report that a session has ended (card removed).
bool ChangeStatus;                       //Set to 1 to iindicate to send a report that the machine's state has changed, outside of a session start/end.
String FEVer;                            //Stores the version number of the frontend.
uint64_t LastServer;                     //Tracks the last time we had a good communication from the server
bool CheckNetwork;                       //Flag to indicate repeat network communication failures. Needs investigating.
uint64_t SessionStart;                   //Time in millis when a session started.
uint64_t SessionTime;                    //How long a user has been using a machine for
uint64_t LastSessionTime;                //Duration of the last session.
bool LogoffSent;                         //Flag to indicate that the system has sent the message to end a session, so data can be cleared.
String State = "Lockout";                //Plaintext indication of the state of the system for status reports. Idle, Unlocked, AlwayOn, or Lockout.
bool CardVerified;                       //Flag set to 1 once the results of the ID check are complete and CardStatus is valid.
bool CardStatus;                         //Set to 1 if a card is authorized to use the machine, 0 if not,
bool InternalVerified;                   //Set to 1 when the card has been verified against the internal list, and InternalStatus is valid.
bool InternalStatus;                     //Set to 1 if the card is authorized to use the machine based on the internal list.
bool PollCheck;                          //Flag indicating it is time for a regular frontend poll
bool NewLED;                             //Flag that there is new LED information to send
byte Red;                                //Red channel of LED
byte Green;                              //Green channel of LED
byte Blue;                               //Blue channel of LED
bool NewBuzzer;                          //Flag that there is new buzzer to send
int Tone;                                //Frequency of buzzer to play
bool Switch;                             //Value of the switch to sent to frontend
bool DebugLED;                           //Turns on/off the debugLED on the frontend
bool NoNetwork = 1;                      //Indicates inability to connect to the server
bool TemperatureFault;                   //1 Indicates an overtemperature condition
bool Fault;                              //Flag indicates a failure of some sort with the system other than temperature. 
bool CardUnread;                         //A card is preesnt in the machine but hasn't been read yet. 
bool VerifiedBeep;                       //Flag, set to 1 to sound a beep when a card is verified. Lets staff knw it is valid to use their key when the machine is in a non-idle state.
bool ReadError;                          //Flag, set 1 when we fail to read a card to flash lights/buzzers.
bool NoBuzzer;                           //Flag, set to 1 to mute the buzzer.
bool ResetLED;                           //Flag, set to 1 to show a purple light on the front indicating a reset
String ResetReason = "Unknown";          //Plaintext message of the reason for the reset, to be reported to the server.
unsigned long MessageNumber = 0;         //Tracks the number of messages sent via websockets
String WSIncoming;                       //The new message received from the websocket
bool NewFromWS;                          //1 indicates there's a message to process
bool VerifyID;                           //1 indicates we should verify the card present against the server
bool SendWSReconnect;                    //1 indicates we've (re)connected to the server, so send the preamble message again
bool WSSend = 0;                         //1 if there is already a websocket message primed to send out
bool UseEthernet = 0;                    //1 if we should be using ethernet
bool UseWiFi = 0;                        //1 if we should be using wifi
bool EthernetPresent = 0;                //Flag if there is ethrtnet hardware installed.
char InterfaceUsed;                      //0 if using WiFi, 1 if using Ethernet.
String PreState;                         //What state the system was in right before a keycard is inserted, to prevent glitches to the state.
String SerialNumber;                     //Plaintext store of the shlug identifier from OneWire
bool JustDisconnected;                   //Lets us detect if a websocket was just dropped or has been for a bit.
bool ChangeBeep;                         //Flag to beep if the state has been remotely changed.
String PreUnlockState;                   //State the machine was in before being unlocked.
bool Identify;                           //Set to 1 to play a constant noise and lighting to find the device. Useful in websocket setup.
byte Brightness = 255;                   //Overarching setting that sets the LED brightness
uint64_t LastLightChange;                //Tracks when the last time the lighting was changed.
bool OTATimeout;                         //Set to 1 if we checked the OTA timeout
String StateSource = "Startup";          //Logs what caused the state to change for reporting.
uint64_t NextPing;                       //When we should send the next ping to see if we are connected to the server.
bool PingPending;                        //If 1, we are waiting to hear back from a ping
uint64_t PingTimeout;                    //If we reach this time, it has been too long since we sent the ping.
bool SecondPing;                         //Tracks how many pings we've missed
String SocketText;                       //Stores the text message to be sent via the websocket
bool InternetOK;                         //1 if we have an OK connection to the internet, not necessarily the server though.
bool DisconnectWebsocket;                //Set to 1 to disconnect websockets.
bool SendPing;                           //Set to 1 to send a ping
byte SocketRetry;                        //Count of how many times we retried to send a message.
uint64_t NextSocketTry;                  //The time we should try to send the next message.
uint64_t WebsocketResetTime;             //When we should reconnect the websocket.
bool ConnectWebsocket;                   //Set to 1 to trigger a connection.
bool SecondMessageFail;                  //Tracks how many outgoing messages have been lost.
bool NightlyFlag;                        //Flag indicates it is past 4am and time to restart next time in an OK state.
uint64_t NextNetworkCheck;               //Time when we next check the network for connectivity
bool SecondNetworkFail;                  //Bool to check if we have had network issues in the past.

//Libraries:
#include <OneWireESP32.h>         //Version 2.0.2 | Source: https://github.com/junkfix/esp32-ds18b20
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

//Objects:
Adafruit_PN532 nfc(SCKPin, MISOPin, MOSIPin, NFCCS);
Preferences settings;
WiFiClientSecure client;
HardwareSerial Internal(1);
JsonDocument usbjson;
HTTPClient http;
ESP32OTAPull ota;
TaskHandle_t xHandle;
TaskHandle_t xHandle2;
ESP32Time rtc;

WebSocketsClient socket;

//Mutexes:
SemaphoreHandle_t DebugMutex; //Reserves the USB serial output, priamrily for debugging purposes.
SemaphoreHandle_t NetworkMutex; //Reserves the network connection
SemaphoreHandle_t OneWireMutex; //Reserves the OneWire connection. Currently only used for temperature measurement, eventually will measure system integrity.
SemaphoreHandle_t StateMutex; //Reserves the State string, since it takes a long time to change.
SemaphoreHandle_t MessageMutex; //Reserves the ability to send a message.

extern "C" bool verifyRollbackLater() {
  //This code is run to verify the OTA before actual setup.
  //Since we are handling OTA verification ourselves, we just return true.
  return true;
}

void setup(){
  xTaskCreate(OTAWatchdog, "OTAWatchdog", 2048, NULL, 1, NULL);
	delay(100);
  
  //Create mutexes:
  DebugMutex = xSemaphoreCreateMutex();
  NetworkMutex = xSemaphoreCreateMutex();
  OneWireMutex = xSemaphoreCreateMutex();
  StateMutex = xSemaphoreCreateMutex();
  MessageMutex = xSemaphoreCreateMutex();

  Internal.begin(115200, SERIAL_8N1, TOESP, TOTINY);

  //Flash the lights on the front to indicate we are in startup
  xTaskCreate(GamerMode, "GamerMode", 2048, NULL, 5, &xHandle2);

  delay(5000); 

  //We want to start the USBConfig early, so we can send new credentials for WiFi and the like before startup.
  #ifndef WebsocketUART
    Serial.println(F("USB Settings Input Started."));
    xTaskCreate(USBConfig, "USBConfig", 4096, NULL, 2, NULL);
  #endif

  if(TXINTERRUPT){
    Serial.begin(115200, SERIAL_8N1, 44, DB9INT);
  } else{
    Serial.begin(115200);
  }
  Serial.println(F("STARTUP"));

  //First, load all settings from memory
  settings.begin("settings", false);
  Server = settings.getString("Server");
  if(Server == NULL){
    Serial.println(F("CAN'T FIND SETTINGS - FRESH INSTALL?"));
    Serial.println(F("HOLDING FOR UPDATE FOREVER..."));
    //Nuke the rest of this process - we can't do anything without our config.
    vTaskSuspend(NULL);
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
  TempLimit = settings.getString("TempLimit").toInt();
  if(TempLimit == 50){
    //Increase from default
    TempLimit = 65;
    settings.putString("TempLimit",String(TempLimit));
  }
  Frequency = settings.getString("Frequency").toInt();
  NetworkMode = settings.getString("NetworkMode").toInt();
  NoBuzzer = settings.getString("NoBuzzer").toInt();

  //New setting added in V1.3.2. Need to check if it exists, if not make with a default value
  if(settings.isKey("Brightness")){
    Brightness = settings.getString("Brightness").toInt();
  } else{
    Brightness = 255;
    settings.putString("Brightness",String(Brightness));
  }

  //New setting in V1.3.8, timezone. Set to EST if not present
  int TimezoneHr;
  if(settings.isKey("Timezone")){
    TimezoneHr = settings.getString("Timezone").toInt();
  } else{
    TimezoneHr = -4;
  }
  rtc.offset = TimezoneHr * 3600;

  //Go through and delete any keys we no longer use from old versions. 
  DeleteOld("SecurityCode");
  DeleteOld("MachineName");
  DeleteOld("MachineID");
  DeleteOld("MachineType");
  DeleteOld("SwitchType");
  DeleteOld("Zone");
  DeleteOld("NeedsWelcome");

  //Start the network connection;
  xTaskCreate(NetworkManager, "NetworkManager", 2048, NULL, 2, NULL);

  //We should wait for the network before continuing.
  while(NoNetwork){
    delay(50);
  }
  //Reset NoNetwork to its default value of 1
  NoNetwork = 1;

  //Set the ResetReason to OTA in case we restart now
  ResetReason = settings.getString("ResetReason");
  settings.putString("ResetReason","OTA-Update");

  //Then, check for an OTA update.
  if(DebugMode){
    Serial.println(F("Checking for OTA Updates..."));
    Serial.println(F("If any are found, will install immediately."));
  }
  ota.SetCallback(callback_percent);
  ota.SetConfig(Hardware);
  ota.OverrideDevice("ACS Core");
  if(DebugMode){
    ota.EnableSerialDebug();
  }
  int otaresp = ota.CheckForOTAUpdate(OTA_URL, Version);
  if(DebugMode){
    Serial.print(F("OTA Response Code: ")); Serial.println(otaresp);
    Serial.println(errtext(otaresp));
    Serial.println(F("We're still here, so there must not have been an update."));
  }
  
  settings.putString("ResetReason","Unknown");

  //Check to make sure we can connect to the NFC reader and it isn't damaged.
  pinMode(NFCPWR, OUTPUT);
  pinMode(NFCRST, OUTPUT);
  digitalWrite(NFCPWR, HIGH);
  digitalWrite(NFCRST, HIGH);
  delay(100);
  bool StartupFault;
  retry : 
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Didn't find PN532 board. Trying again...");
    delay(250);
    vTaskDelete(xHandle2); //Delete the Gamer mode lights
    if(StartupFault){
      Internal.println(F("L 255,0,0"));
    } else{
      Internal.println(F("L 0,0,0"));
    }
    Internal.flush();
    StartupFault =! StartupFault;
    goto retry;
  }
  digitalWrite(NFCRST, LOW);
  digitalWrite(NFCPWR, LOW);
  if(DebugMode){
    Serial.println(F("NFC Setup")); 
  }
  //Load up the SPIFFS file of verified IDs, format/create if it is not there.
  if(!SPIFFS.begin(1)){ //Format SPIFFS if fails
    Serial.println("SPIFFS Mount Failed");
    while(1);
  }
  if(SPIFFS.exists("/validids.txt")){
    Serial.println(F("Valid ID List already exists."));
  } else{
    Serial.println(F("No Valid ID List file found. Creating..."));
    File file = SPIFFS.open("/validids.txt", "w");
    if(!file){
      Serial.println(F("ERROR: Unable to make file."));
    }
    file.print(F("Valid IDs:"));
    file.println();
    file.close();
  }

  //Set the inital state
  //If the ESP32 performed a controlled restart, such as to install an OTA update or try to fix a wireless issue, 
  Serial.print(F("Reset Reason: "));
  print_reset_reason(rtc_get_reset_reason(0));
  if(rtc_get_reset_reason(0) != POWERON_RESET){
    //Reset for a reason other than power on reset.
    State = settings.getString("LastState");
    if(State == "" || State == NULL || State == "Restart"){
      //There wasn't a state to retrieve?
      State = "Startup";
    }
  } else{
    State = "Startup";
    ResetReason = "Power-On";
  }
  if(State == "Fault"){
    //Shouldn't return to a fault state
    State = "Startup";
  }
  if(DebugMode){
    Serial.print(F("Set State: "));
    Serial.println(State);
  }

  //Check for a possible panic crash, report it.
  if(ResetReason == "Unknown" && rtc_get_reset_reason(0) == RTC_SW_CPU_RESET){
    ResetReason = "Panic-Crash";
  }
  if(rtc_get_reset_reason(0) == TG1WDT_SYS_RESET){
    //Definitely a panic crash
    ResetReason = "Panic-Crash";
  }
  if(rtc_get_reset_reason(0) == 3){
    //Likely a brownout
    ResetReason = "3-Maybe-Power-Issues";
  }
  if(ResetReason == "Unknown"){
    //Append the reset reason for debug
    ResetReason = "Unknown-Type-" + String(rtc_get_reset_reason(0));
  }

  //Report the reset reason;
  Message = "Restart-Reason-" + ResetReason;
  ReadyToSend = 1;

  //Disable the startup lights
  vTaskDelete(xHandle2);

  //Lastly, create all tasks and begin operating normally.
  vTaskSuspendAll();
  xTaskCreate(RegularReport, "RegularReport", 1024, NULL, 6, NULL); 
  xTaskCreate(Temperature, "Temperature", 2048, NULL, 5, NULL);
  xTaskCreate(USBConfig, "USBConfig", 2048, NULL, 5, NULL);
  xTaskCreate(InternalRead, "InternalRead", 1500, NULL, 5, NULL);
  xTaskCreate(ReadCard, "ReadCard", 2048, NULL, 5, NULL);
  //InternalWrite has a handle to allow it to be suspended before a restart (disables lighting changes)
  xTaskCreate(InternalWrite, "InternalWrite", 2048, NULL, 5, &xHandle);
  xTaskCreate(LEDControl, "LEDControl", 1024, NULL, 5, NULL);
  xTaskCreate(BuzzerControl, "BuzzerControl", 1024, NULL, 5, NULL);
  xTaskCreate(MachineState, "MachineState", 2048, NULL, 5, NULL);
  xTaskCreate(SocketManager, "SocketManager", 4096, NULL, 3, NULL);
  xTaskResumeAll();

  Serial.println(F("Setup done."));
}

void loop(){
  //Check for new websocket messages constantly
  if(InternetOK){
    if((WebsocketResetTime != 0) && (millis64() <= WebsocketResetTime)){
      WebsocketResetTime = 0;
      ConnectWebsocket = 1;
    }
    if(ConnectWebsocket){
      if(!socket.isConnected()){
        StartWebsocket();
      }
      ConnectWebsocket = 0;
    }
    if(DisconnectWebsocket){
      socket.disconnect();
      DisconnectWebsocket = 0;
      WebsocketResetTime = millis64() + 5000;
    } else{
      if(SocketText != ""){
        //There is a websocket message in the outbox.
        if(millis64() >= NextSocketTry){
          if(!socket.sendTXT(SocketText)){
            //Was unable to send message
            SocketRetry = SocketRetry + 1;
            //Set the time to retry
            //Try 200ms later on the first fail, 400ms next, etc.
            NextSocketTry = millis64() + (SocketRetry * 400);
            if(SocketRetry >= 15){
              //We've failed way too many times
              NoNetwork = 1;
              SocketText = "";
              Message = "Socket failed to send message 15 times over 5 seconds!";
              ReadyToSend = 1;
              NoNetwork = 1;
              if(!SecondMessageFail){
                SecondMessageFail = 1;
              } else{
                DisconnectWebsocket = 1;
                SecondMessageFail = 0;
              }
            }
          } else{
            if(DebugMode){
              Serial.print(F("Websocket payload sent after "));
              Serial.print(SocketRetry);
              Serial.println(F(" attempts."));
            }
            SocketText = "";
            NextSocketTry = millis64();
            if(SocketRetry > 0){
              if(!LogReadyToSend){
                //No message pending, so send one
                LogKey = "RetryLast";
                LogValue = String(SocketRetry);
                LogReadyToSend = 1;
              }
            }
            SocketRetry = 0;
          }
        }
      }
      if(SendPing){
        socket.sendPing();
        SendPing = 0;
      }
      socket.loop();
    }
  }
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

void print_reset_reason(int reason) {
  switch (reason) {
    case 1:  Serial.println("POWERON_RESET"); break;          /**<1,  Vbat power on reset*/
    case 3:  Serial.println("SW_RESET"); break;               /**<3,  Software reset digital core*/
    case 4:  Serial.println("OWDT_RESET"); break;             /**<4,  Legacy watch dog reset digital core*/
    case 5:  Serial.println("DEEPSLEEP_RESET"); break;        /**<5,  Deep Sleep reset digital core*/
    case 6:  Serial.println("SDIO_RESET"); break;             /**<6,  Reset by SLC module, reset digital core*/
    case 7:  Serial.println("TG0WDT_SYS_RESET"); break;       /**<7,  Timer Group0 Watch dog reset digital core*/
    case 8:  Serial.println("TG1WDT_SYS_RESET"); break;       /**<8,  Timer Group1 Watch dog reset digital core*/
    case 9:  Serial.println("RTCWDT_SYS_RESET"); break;       /**<9,  RTC Watch dog Reset digital core*/
    case 10: Serial.println("INTRUSION_RESET"); break;        /**<10, Instrusion tested to reset CPU*/
    case 11: Serial.println("TGWDT_CPU_RESET"); break;        /**<11, Time Group reset CPU*/
    case 12: Serial.println("SW_CPU_RESET"); break;           /**<12, Software reset CPU*/
    case 13: Serial.println("RTCWDT_CPU_RESET"); break;       /**<13, RTC Watch dog Reset CPU*/
    case 14: Serial.println("EXT_CPU_RESET"); break;          /**<14, for APP CPU, reset by PRO CPU*/
    case 15: Serial.println("RTCWDT_BROWN_OUT_RESET"); break; /**<15, Reset when the vdd voltage is not stable*/
    case 16: Serial.println("RTCWDT_RTC_RESET"); break;       /**<16, RTC Watch dog reset digital core and rtc module*/
    default: Serial.println("NO_MEAN");
  }
}

uint64_t millis64(){
  //This simple function replaces the 32 bit default millis. Means that overflow now occurs in 290,000 years instead of 50 days
  //Timer runs in microseocnds, so divide by 1000 to get millis.
  return esp_timer_get_time() / 1000;
}

void ReadyMessage(String ToSend){
  //This function can be used to pass a message to the "MessageReport" task.
  while(WritingMessage){
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
  WritingMessage = 1;
  xSemaphoreTake(MessageMutex, portMAX_DELAY); 
  //If we have the mutex, and we have WritingMessage, we can write our message and set the flag.
  Message = ToSend;
  ReadyToSend = 1;
  xSemaphoreGive(MessageMutex);
}

void GamerMode(void *pvParameters){
  //This task simply blinks the front LED red-green-blue, aka gamer mode, to indicate startup in progress.
  while(1){
    Internal.println("L 255,0,0");
    delay(LEDFlashTime);
    Internal.println("L 0,255,0");
    delay(LEDFlashTime);
    Internal.println("L 0,0,255");
    delay(LEDFlashTime);
  }
}

void RegularReport(void *pvParameters){
  //This sub-task is just to set the regular report flag periodically
  while(1){
    RegularStatus = 1;
    vTaskDelay(Frequency*1000 / portTICK_PERIOD_MS);
  }
}

void OTAWatchdog(void *pvParameters){
  while(1){
    //This task reverts an OTA if not marked valid in 60 seconds.
    if(!OTATimeout && (millis64() >= 60000)){
      OTATimeout = 1;
      const esp_partition_t *running_partition = esp_ota_get_running_partition();
      esp_ota_img_states_t ota_state;
      esp_ota_get_state_partition(running_partition, &ota_state);
      if(ota_state == ESP_OTA_IMG_PENDING_VERIFY){
        Serial.println(F("OTA update timer failed after 60 seconds."));
        Serial.println(F("Reverting firmware."));
        settings.putString("ResetReason","OTA-Revert");
        esp_ota_mark_app_invalid_rollback_and_reboot();
      } else{
        Serial.println(F("OTA update timer passed. Disabling."));
        vTaskSuspend(NULL);
      }
    }
  }
}

void DeleteOld(String KeyName){
  //Delete the old key if it exists
  if(settings.isKey(KeyName.c_str())){
    settings.remove(KeyName.c_str());
    Serial.print("Deleted old setting: ");
    Serial.println(KeyName);
  }
}
